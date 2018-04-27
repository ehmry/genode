/*
 * \brief  Sculpt system manager
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <nitpicker_session/connection.h>

/* included from depot_deploy tool */
#include <children.h>

/* local includes */
#include "child_exit_state.h"
#include "storage_dialog.h"
#include "network_dialog.h"
#include "gui.h"
#include "nitpicker.h"
#include "gen_e2fs.h"
#include "gen_file_system.h"
#include "gen_ram_fs.h"
#include "gen_file_browser.h"
#include "gen_nic_drv.h"
#include "gen_wifi_drv.h"
#include "gen_nic_router.h"
#include "gen_prepare.h"
#include "gen_chroot.h"
#include "gen_update.h"
#include "gen_fs_rom.h"
#include "gen_depot_query.h"
#include "gen_gpt_write.h"
#include "discovery_state.h"

namespace Sculpt_manager { struct Main; }

struct Sculpt_manager::Main : Input_event_handler,
                              Storage_dialog::Action,
                              Network_dialog::Action
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Nitpicker::Connection _nitpicker { _env, "input" };

	Signal_handler<Main> _input_handler {
		_env.ep(), *this, &Main::_handle_input };

	void _handle_input()
	{
		_nitpicker.input()->for_each_event([&] (Input::Event const &ev) {
			handle_input_event(ev); });
	}

	Expanding_reporter _fonts_config { _env, "config", "fonts_config" };

	Signal_handler<Main> _nitpicker_mode_handler {
		_env.ep(), *this, &Main::_handle_nitpicker_mode };

	void _handle_nitpicker_mode();

	Attached_rom_dataspace _nitpicker_hover { _env, "nitpicker_hover" };

	Signal_handler<Main> _nitpicker_hover_handler {
		_env.ep(), *this, &Main::_handle_nitpicker_hover };

	void _handle_nitpicker_hover();


	/***************************
	 ** Configuration loading **
	 ***************************/

	Prepare_version _prepare_version   { 0 };
	Prepare_version _prepare_completed { 0 };

	bool _prepare_in_progress() const
	{
		return _prepare_version.value != _prepare_completed.value;
	}


	/*************
	 ** Storage **
	 *************/

	Attached_rom_dataspace _block_devices_rom { _env, "block_devices" };

	Attached_rom_dataspace _usb_active_config_rom { _env, "usb_active_config" };

	Storage_devices _storage_devices { };

	Ram_fs_state _ram_fs_state { };

	Storage_target _sculpt_partition { };

	Discovery_state _discovery_state { };

	File_browser_version _file_browser_version { 0 };

	Storage_dialog _storage_dialog {
		_env, _storage_devices, _ram_fs_state, _sculpt_partition };

	void _handle_storage_devices_update();

	Signal_handler<Main> _storage_device_update_handler {
		_env.ep(), *this, &Main::_handle_storage_devices_update };

	template <typename FN>
	void _apply_partition(Storage_target const &target, FN const &fn)
	{
		_storage_devices.for_each([&] (Storage_device &device) {

			if (target.device != device.label)
				return;

			device.for_each_partition([&] (Partition &partition) {

				bool const whole_device = !target.partition.valid()
				                       && !partition.number.valid();

				bool const partition_matches = (device.label == target.device)
				                            && (partition.number == target.partition);

				if (whole_device || partition_matches) {
					fn(partition);
					_generate_runtime_config();
				}
			});
		});
	}


	/**
	 * Storage_dialog::Action interface
	 */
	void format(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.format_in_progress = true; });
	}

	void cancel_format(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.file_system.type   = File_system::UNKNOWN;
			partition.format_in_progress = false;
			_storage_dialog.reset_operation();
		});
	}

	void expand(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.gpt_expand_in_progress = true; });
	}

	void cancel_expand(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.file_system.type       = File_system::UNKNOWN;
			partition.gpt_expand_in_progress = false;
			partition.fs_resize_in_progress  = false;
			_storage_dialog.reset_operation();
		});
	}

	void check(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.check_in_progress = true; });
	}

	void toggle_file_browser(Storage_target const &target) override
	{
		File_browser_version const orig_version = _file_browser_version;

		if (target.ram_fs()) {
			_ram_fs_state.inspected = !_ram_fs_state.inspected;
			_file_browser_version.value++;
		}

		_apply_partition(target, [&] (Partition &partition) {
			partition.file_system_inspected = !partition.file_system_inspected;
			_file_browser_version.value++;
		});

		if (orig_version.value == _file_browser_version.value)
			return;

		_generate_runtime_config();
	}

	void toggle_default_storage_target(Storage_target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.toggle_default_label(); });
	}

	void use(Storage_target const &target) override
	{
		_sculpt_partition = target;

		/* trigger loading of the configuration from the sculpt partition */
		_prepare_version.value++;

		_generate_runtime_config();
	}

	void reset_ram_fs() override
	{
		_ram_fs_state.ram_quota = Ram_fs_state::initial_ram();
		_ram_fs_state.version.value++;

		_storage_dialog.reset_operation();
		_generate_runtime_config();
	}


	/*************
	 ** Network **
	 *************/

	Nic_target _nic_target { };
	Nic_state  _nic_state  { };

	bool _use_nic_drv  = false;
	bool _use_wifi_drv = false;

	Attached_rom_dataspace _wlan_accesspoints_rom {
		_env, "report -> runtime/wifi_drv/wlan_accesspoints" };

	Attached_rom_dataspace _wlan_state_rom {
		_env, "report -> runtime/wifi_drv/wlan_state" };

	Attached_rom_dataspace _nic_router_state_rom {
		_env, "report -> runtime/nic_router/state" };

	Access_points _access_points { };

	Wifi_connection _wifi_connection = Wifi_connection::disconnected_wifi_connection();

	void _handle_wlan_accesspoints();
	void _handle_wlan_state();
	void _handle_nic_router_state();

	Signal_handler<Main> _wlan_accesspoints_handler {
		_env.ep(), *this, &Main::_handle_wlan_accesspoints };

	Signal_handler<Main> _wlan_state_handler {
		_env.ep(), *this, &Main::_handle_wlan_state };

	Signal_handler<Main> _nic_router_state_handler {
		_env.ep(), *this, &Main::_handle_nic_router_state };

	Network_dialog _network_dialog {
		_env, _nic_target, _access_points, _wifi_connection, _nic_state };

	Expanding_reporter _wlan_config { _env, "selected_network", "wlan_config" };

	/**
	 * Network_dialog::Action interface
	 */
	void nic_target(Nic_target const &nic_target) override
	{
		log("select NIC ", (int)nic_target.type);

		/*
		 * Start drivers on first use but never remove them to avoid
		 * driver-restarting issues.
		 */
		if (nic_target.type == Nic_target::WIFI)
			_use_wifi_drv = true;

		if (nic_target.type == Nic_target::WIRED)
			_use_nic_drv = true;

		_nic_target = nic_target;
		_generate_runtime_config();
	}

	void wifi_disconnect() override
	{
		/*
		 * Reflect state change immediately to the user interface even
		 * if the wifi driver will take a while to perform the disconnect.
		 */
		_wifi_connection = Wifi_connection::disconnected_wifi_connection();

		_wlan_config.generate([&] (Xml_generator &xml) {

			/* generate attributes to ease subsequent manual tweaking */
			xml.attribute("ssid", "");
			xml.attribute("protection", "WPA-PSK");
			xml.attribute("psk", "");
		});

		_generate_runtime_config();
	}


	/************
	 ** Update **
	 ************/

	Attached_rom_dataspace _fetchurl_progress_rom {
		_env, "report -> runtime/update/dynamic/fetchurl/progress" };

	void _handle_fetchurl_progress();

	Signal_handler<Main> _fetchurl_progress_handler {
		_env.ep(), *this, &Main::_handle_fetchurl_progress };


	/************
	 ** Deploy **
	 ************/

	typedef String<16> Arch;
	Arch _arch { };

	struct Depot_rom_state { Ram_quota ram_quota; };

	Depot_rom_state _depot_rom_state { 32*1024*1024 };

	Attached_rom_dataspace _manual_deploy_rom { _env, "manual_deploy_config" };

	Attached_rom_dataspace _blueprint_rom { _env, "report -> runtime/depot_query/blueprint" };

	Expanding_reporter _depot_query_reporter { _env, "query" , "depot_query"};

	Depot_deploy::Children _children { _heap };

	void _handle_deploy();

	Signal_handler<Main> _deploy_handler {
		_env.ep(), *this, &Main::_handle_deploy };


	/************
	 ** Global **
	 ************/

	Gui _gui { _env };

	Expanding_reporter _focus_reporter { _env, "focus", "focus" };

	Attached_rom_dataspace _runtime_state { _env, "runtime_state" };

	Expanding_reporter _runtime_config { _env, "config", "runtime_config" };

	inline void _generate_runtime_config(Xml_generator &) const;

	void _generate_runtime_config()
	{
		_runtime_config.generate([&] (Xml_generator &xml) {
			_generate_runtime_config(xml); });
	}

	Signal_handler<Main> _runtime_state_handler {
		_env.ep(), *this, &Main::_handle_runtime_state };

	void _handle_runtime_state();

	/**
	 * Input_event_handler interface
	 */
	void handle_input_event(Input::Event const &ev) override
	{
		if (ev.key_press(Input::BTN_LEFT)) {
			if (_storage_dialog.hovered()) _storage_dialog.click(*this);
			if (_network_dialog.hovered()) _network_dialog.click(*this);
		}

		if (ev.key_release(Input::BTN_LEFT))
			_storage_dialog.clack(*this);
	}

	Nitpicker::Root _gui_nitpicker { _env, _heap, *this };

	Main(Env &env) : _env(env)
	{
		_focus_reporter.generate([&] (Xml_generator &xml) {
//			xml.attribute("label", "manager -> input"); });
			xml.attribute("label", "wm -> "); });

		_runtime_state.sigh(_runtime_state_handler);

		_nitpicker.input()->sigh(_input_handler);
		_nitpicker.mode_sigh(_nitpicker_mode_handler);

		/*
		 * Adjust GUI parameters to initial nitpicker mode
		 */
		_handle_nitpicker_mode();

		/*
		 * Generate initial configurations
		 */
		wifi_disconnect();

		/*
		 * Subscribe to reports
		 */
		_block_devices_rom.sigh    (_storage_device_update_handler);
		_usb_active_config_rom.sigh(_storage_device_update_handler);
		_wlan_accesspoints_rom.sigh(_wlan_accesspoints_handler);
		_wlan_state_rom.sigh       (_wlan_state_handler);
		_nic_router_state_rom.sigh (_nic_router_state_handler);
		_fetchurl_progress_rom.sigh(_fetchurl_progress_handler);
		_manual_deploy_rom.sigh    (_deploy_handler);
		_blueprint_rom.sigh        (_deploy_handler);
		_nitpicker_hover.sigh      (_nitpicker_hover_handler);

		/*
		 * Import initial report content
		 */
		_handle_nitpicker_hover();
		_handle_storage_devices_update();
		_handle_deploy();

		_generate_runtime_config();

		_storage_dialog.generate();
		_network_dialog.generate();

		_gui.generate_config();
	}
};


void Sculpt_manager::Main::_handle_nitpicker_mode()
{
	Framebuffer::Mode const mode = _nitpicker.mode();

	_fonts_config.generate([&] (Xml_generator &xml) {
		xml.node("vfs", [&] () {
			gen_named_node(xml, "rom", "Vera.ttf");
			gen_named_node(xml, "rom", "VeraMono.ttf");
			gen_named_node(xml, "dir", "fonts", [&] () {

				auto gen_ttf_dir = [&] (char const *dir_name,
				                        char const *ttf_path, float size_px) {

					gen_named_node(xml, "dir", dir_name, [&] () {
						gen_named_node(xml, "ttf", "regular", [&] () {
							xml.attribute("path",    ttf_path);
							xml.attribute("size_px", size_px);
							xml.attribute("cache",   "256K");
						});
					});
				};

				float const text_size = (float)mode.height() / 60.0;

				gen_ttf_dir("title",      "/Vera.ttf",     text_size*1.25);
				gen_ttf_dir("text",       "/Vera.ttf",     text_size);
				gen_ttf_dir("annotation", "/Vera.ttf",     text_size*0.8);
				gen_ttf_dir("monospace",  "/VeraMono.ttf", text_size);
			});
		});
		xml.node("default-policy", [&] () { xml.attribute("root", "/fonts"); });
	});

	_gui.version.value++;
	_gui.generate_config();
}


void Sculpt_manager::Main::_handle_nitpicker_hover()
{
	if (!_discovery_state.discovery_in_progress())
		return;

	_nitpicker_hover.update();

	if (_nitpicker_hover.xml().attribute_value("active", false) == true)
		_discovery_state.user_intervention = true;
}


void Sculpt_manager::Main::_handle_storage_devices_update()
{
	bool reconfigure_runtime = false;
	{
		_block_devices_rom.update();
		Block_device_update_policy policy(_env, _heap, _storage_device_update_handler);
		_storage_devices.update_block_devices_from_xml(policy, _block_devices_rom.xml());

		_storage_devices.block_devices.for_each([&] (Block_device &dev) {

			dev.process_part_blk_report();

			if (dev.state == Storage_device::UNKNOWN) {
				reconfigure_runtime = true; };
		});
	}

	{
		_usb_active_config_rom.update();
		Usb_storage_device_update_policy policy(_env, _heap, _storage_device_update_handler);
		Xml_node const config = _usb_active_config_rom.xml();
		Xml_node const raw = config.has_sub_node("raw")
		                   ? config.sub_node("raw") : Xml_node("<raw/>");

		_storage_devices.update_usb_storage_devices_from_xml(policy, raw);

		_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device &dev) {

			dev.process_driver_report();
			dev.process_part_blk_report();

			if (dev.state == Storage_device::UNKNOWN) {
				reconfigure_runtime = true; };
		});
	}

	if (!_sculpt_partition.valid()) {

		Storage_target const default_target =
			_discovery_state.detect_default_target(_storage_devices);

		if (default_target.valid())
			use(default_target);
	}

	_storage_dialog.generate();

	if (reconfigure_runtime)
		_generate_runtime_config();
}


void Sculpt_manager::Main::_handle_wlan_accesspoints()
{
	bool const initial_scan = !_wlan_accesspoints_rom.xml().has_sub_node("accesspoint");

	_wlan_accesspoints_rom.update();

	/* suppress updating the list while the access-point list is hovered */
	if (!initial_scan && _network_dialog.ap_list_hovered())
		return;

	Access_point_update_policy policy(_heap);
	_access_points.update_from_xml(policy, _wlan_accesspoints_rom.xml());
	_network_dialog.generate();
}


void Sculpt_manager::Main::_handle_wlan_state()
{
	_wlan_state_rom.update();
	_wifi_connection = Wifi_connection::from_xml(_wlan_state_rom.xml());
	_network_dialog.generate();
}


void Sculpt_manager::Main::_handle_nic_router_state()
{
	_nic_router_state_rom.update();

	Nic_state const old_nic_state = _nic_state;
	_nic_state = Nic_state::from_xml(_nic_router_state_rom.xml());
	_network_dialog.generate();

	/* if the nic state becomes ready, consider spawning the update subsystem */
	if (old_nic_state.ready() != _nic_state.ready())
		_generate_runtime_config();
}


void Sculpt_manager::Main::_handle_fetchurl_progress()
{
	_fetchurl_progress_rom.update();

	Xml_node progress = _fetchurl_progress_rom.xml();
	progress.for_each_sub_node("fetch", [&] (Xml_node fetch) {

		typedef String<128> Url;
		Url   const url   = fetch.attribute_value("url",   Url());
		float const total = fetch.attribute_value("total", 0.0);
		float const now   = fetch.attribute_value("now",   0.0);
		if (now != total && total > 0) {
			log(url, " ", (100*now)/total, "%");
		}
	});
}


void Sculpt_manager::Main::_handle_runtime_state()
{
	_runtime_state.update();

	Xml_node state = _runtime_state.xml();

	bool reconfigure_runtime = false;

	/* check for completed storage operations */
	_storage_devices.for_each([&] (Storage_device &device) {

		device.for_each_partition([&] (Partition &partition) {

			Storage_target const target { device.label, partition.number };

			if (partition.check_in_progress) {
				String<64> name(target.label(), ".fsck.ext2");
				Child_exit_state exit_state(state, name);

				if (exit_state.exited) {
					if (exit_state.code != 0)
						error("file-system check failed");
					if (exit_state.code == 0)
						log("file-system check succeeded");

					partition.check_in_progress = 0;
					reconfigure_runtime = true;
					_storage_dialog.reset_operation();
				}
			}

			if (partition.format_in_progress) {
				String<64> name(target.label(), ".mkfs.ext2");
				Child_exit_state exit_state(state, name);

				if (exit_state.exited) {
					if (exit_state.code != 0)
						error("file-system creation failed");

					partition.format_in_progress = false;
					partition.file_system.type = File_system::EXT2;

					if (partition.whole_device())
						device.rediscover();

					reconfigure_runtime = true;
					_storage_dialog.reset_operation();
				}
			}

			/* respond to completion of file-system resize operation */
			if (partition.fs_resize_in_progress) {
				Child_exit_state exit_state(state, Start_name(target.label(), ".resize2fs"));
				if (exit_state.exited) {
					partition.fs_resize_in_progress = false;
					reconfigure_runtime = true;
					device.rediscover();
					_storage_dialog.reset_operation();
				}
			}

		}); /* for each partition */

		/* respond to completion of GPT relabeling */
		if (device.relabel_in_progress()) {
			Child_exit_state exit_state(state, device.relabel_start_name());
			if (exit_state.exited) {
				device.rediscover();
				reconfigure_runtime = true;
				_storage_dialog.reset_operation();
			}
		}

		/* respond to completion of GPT expand */
		if (device.gpt_expand_in_progress()) {
			Child_exit_state exit_state(state, device.expand_start_name());
			if (exit_state.exited) {

				/* kick off resize2fs */
				device.for_each_partition([&] (Partition &partition) {
					if (partition.gpt_expand_in_progress) {
						partition.gpt_expand_in_progress = false;
						partition.fs_resize_in_progress  = true;
					}
				});

				reconfigure_runtime = true;
				_storage_dialog.reset_operation();
			}
		}

	}); /* for each device */

	/* remove prepare subsystem when finished */
	{
		Child_exit_state exit_state(state, "prepare");
		if (exit_state.exited) {
			_prepare_completed = _prepare_version;

			/* trigger update and deploy */
			reconfigure_runtime = true;
		}
	}

	/* upgrade ram_fs quota on demand */
	state.for_each_sub_node("child", [&] (Xml_node child) {

		if (child.attribute_value("name", String<16>()) == "ram_fs"
		 && child.has_sub_node("ram")
		 && child.sub_node("ram").has_attribute("requested")) {

			_ram_fs_state.ram_quota.value *= 2;
			reconfigure_runtime = true;
			_storage_dialog.generate();
		}
	});

	/* upgrade depot_rom quota on demand */
	state.for_each_sub_node("child", [&] (Xml_node child) {

		if (child.attribute_value("name", String<16>()) == "depot_rom"
		 && child.has_sub_node("ram")
		 && child.sub_node("ram").has_attribute("requested")) {

			_depot_rom_state.ram_quota.value *= 2;
			reconfigure_runtime = true;
		}
	});

	if (reconfigure_runtime)
		_generate_runtime_config();
}


void Sculpt_manager::Main::_generate_runtime_config(Xml_generator &xml) const
{
	xml.attribute("verbose", "yes");

	xml.node("report", [&] () {
		xml.attribute("init_ram",   "yes");
		xml.attribute("init_caps",  "yes");
		xml.attribute("child_ram",  "yes");
		xml.attribute("delay_ms",   4*500);
		xml.attribute("buffer",     "64K");
	});

	xml.node("parent-provides", [&] () {
		gen_parent_service<Rom_session>(xml);
		gen_parent_service<Cpu_session>(xml);
		gen_parent_service<Pd_session>(xml);
		gen_parent_service<Rm_session>(xml);
		gen_parent_service<Log_session>(xml);
		gen_parent_service<Timer::Session>(xml);
		gen_parent_service<Report::Session>(xml);
		gen_parent_service<Platform::Session>(xml);
		gen_parent_service<Block::Session>(xml);
		gen_parent_service<Usb::Session>(xml);
		gen_parent_service<::File_system::Session>(xml);
		gen_parent_service<Nitpicker::Session>(xml);
		gen_parent_service<Rtc::Session>(xml);
	});

	xml.node("start", [&] () {
		gen_ram_fs_start_content(xml, _ram_fs_state); });

	auto part_blk_needed_for_use = [&] (Storage_device const &dev) {
		return (_sculpt_partition.device == dev.label)
		     && _sculpt_partition.partition.valid(); };

	_storage_devices.block_devices.for_each([&] (Block_device const &dev) {
	
		if (dev.part_blk_needed_for_discovery()
		 || dev.part_blk_needed_for_access()
		 || part_blk_needed_for_use(dev))

			xml.node("start", [&] () {
				Storage_device::Label const parent { };
				dev.gen_part_blk_start_content(xml, parent); }); });

	_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device const &dev) {

		if (dev.usb_block_drv_needed() || _sculpt_partition.device == dev.label)
			xml.node("start", [&] () {
				dev.gen_usb_block_drv_start_content(xml); });

		if (dev.part_blk_needed_for_discovery()
		 || dev.part_blk_needed_for_access()
		 || part_blk_needed_for_use(dev))

			xml.node("start", [&] () {
				Storage_device::Label const driver = dev.usb_block_drv_name();
				dev.gen_part_blk_start_content(xml, driver);
		});
	});

	_storage_devices.for_each([&] (Storage_device const &device) {

		device.for_each_partition([&] (Partition const &partition) {

			Storage_target const target { device.label, partition.number };

			if (partition.check_in_progress) {
				xml.node("start", [&] () {

					auto gen_args = [&] (Xml_generator &xml) {
						xml.node("arg", [&] () { xml.attribute("value", "-yv"); });
						xml.node("arg", [&] () { xml.attribute("value", "/dev/block"); });
					};

					gen_e2fs_start_content(xml, target, "fsck.ext2", gen_args); });
			}

			if (partition.format_in_progress) {
				xml.node("start", [&] () {

					auto gen_args = [&] (Xml_generator &xml) {
						xml.node("arg", [&] () {
							xml.attribute("value", "/dev/block"); }); };

					gen_e2fs_start_content(xml, target, "mkfs.ext2", gen_args);
				});
			}

			if (partition.fs_resize_in_progress) {
				xml.node("start", [&] () {

					auto gen_args = [&] (Xml_generator &xml) {

						auto gen_arg = [&] (char const *arg) {
							xml.node("arg", [&] () {
								xml.attribute("value", arg); }); };

						gen_arg("-f");
						gen_arg("-p");
						gen_arg("/dev/block");
					};

					gen_e2fs_start_content(xml, target, "resize2fs", gen_args);
				});
			}

			if (partition.file_system.type != File_system::UNKNOWN) {
				if (partition.file_system_inspected || target == _sculpt_partition)
					xml.node("start", [&] () {
						gen_fs_start_content(xml, target, partition.file_system.type); });

				/*
				 * Create alias so that the default file system can be referred
				 * to as "default_fs_rw" without the need to know the name of the
				 * underlying storage target.
				 */
				if (target == _sculpt_partition)
					gen_named_node(xml, "alias", "default_fs_rw", [&] () {
						xml.attribute("child", target.fs()); });
			}

		}); /* for each partition */

		/* relabel partitions if needed */
		if (device.relabel_in_progress())
			xml.node("start", [&] () {
				gen_gpt_relabel_start_content(xml, device); });

		/* expand partitions if needed */
		if (device.expand_in_progress())
			xml.node("start", [&] () {
				gen_gpt_expand_start_content(xml, device); });

	}); /* for each device */

	if (_sculpt_partition.ram_fs())
		gen_named_node(xml, "alias", "default_fs_rw", [&] () {
			xml.attribute("child", "ram_fs"); });

	/*
	 * Determine whether showing the file-system browser or not
	 */
	bool any_file_system_inspected = _ram_fs_state.inspected;
	_storage_devices.for_each([&] (Storage_device const &device) {
		device.for_each_partition([&] (Partition const &partition) {
			any_file_system_inspected |= partition.file_system_inspected; }); });

	/*
	 * Load configuration and update depot config on the sculpt partition
	 */
	if (_sculpt_partition.valid() && _prepare_in_progress())
		xml.node("start", [&] () {
			gen_prepare_start_content(xml, _prepare_version); });

	if (any_file_system_inspected)
		gen_file_browser(xml, _storage_devices, _ram_fs_state, _file_browser_version);

	/*
	 * Spawn chroot instances for accessing '/depot' and '/public'. The
	 * chroot instances implicitly refer to the 'default_fs_rw'.
	 */
	if (_sculpt_partition.valid()) {

		auto chroot = [&] (Start_name const &name, Path const &path, Writeable w) {
			xml.node("start", [&] () {
				gen_chroot_start_content(xml, name, path, w); }); };

		chroot("depot_rw",  "/depot",  WRITEABLE);
		chroot("depot",     "/depot",  READ_ONLY);
		chroot("public_rw", "/public", WRITEABLE);
	}

	/*
	 * Network drivers and NIC router
	 */
	if (_use_nic_drv)
		xml.node("start", [&] () { gen_nic_drv_start_content(xml); });

	if (_use_wifi_drv)
		xml.node("start", [&] () { gen_wifi_drv_start_content(xml); });

	if (_nic_target.type != Nic_target::OFF)
		xml.node("start", [&] () {
			gen_nic_router_start_content(xml, _nic_target); });

	/*
	 * Update subsystem
	 */
	if (_sculpt_partition.valid() && !_prepare_in_progress() && _nic_state.ready())
		xml.node("start", [&] () {
			gen_update_start_content(xml); });

	/*
	 * Deployment infrastructure
	 */
	if (_sculpt_partition.valid() && !_prepare_in_progress()) {

		xml.node("start", [&] () {
			gen_fs_rom_start_content(xml, "depot_rom", "depot",
			                         _depot_rom_state.ram_quota); });

		xml.node("start", [&] () {
			gen_depot_query_start_content(xml); });

		Xml_node const manual_deploy = _manual_deploy_rom.xml();

		/* insert content of '<static>' node as is */
		if (manual_deploy.has_sub_node("static")) {
			Xml_node static_config = manual_deploy.sub_node("static");
			xml.append(static_config.content_base(), static_config.content_size());
		}

		/* generate start nodes for deployed packages */
		if (manual_deploy.has_sub_node("common_routes"))
			_children.gen_start_nodes(xml, manual_deploy.sub_node("common_routes"),
			                          "depot_rom");
	}
}


void Sculpt_manager::Main::_handle_deploy()
{
	_manual_deploy_rom.update();
	_blueprint_rom.update();

	Xml_node const manual_deploy = _manual_deploy_rom.xml();

	try {
		_children.apply_config(manual_deploy);
		_children.apply_blueprint(_blueprint_rom.xml());
	}
	catch (...) {
		error("spurious exception during deploy update"); }

	/* determine CPU architecture of deployment */
	_arch = manual_deploy.attribute_value("arch", Arch());
	if (!_arch.valid())
		warning("manual deploy config lacks 'arch' attribute");

	/* update query for blueprints of all unconfigured start nodes */
	if (_arch.valid()) {
		_depot_query_reporter.generate([&] (Xml_generator &xml) {
			xml.attribute("arch", _arch);
			_children.gen_queries(xml);
		});
	}

	_generate_runtime_config();
}


void Component::construct(Genode::Env &env)
{
	static Sculpt_manager::Main main(env);
}

