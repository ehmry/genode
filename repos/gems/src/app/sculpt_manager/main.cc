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

/* local includes */
#include "child_exit_state.h"
#include "storage_dialog.h"
#include "gui.h"
#include "runtime.h"
#include "nitpicker.h"

namespace Sculpt_manager { struct Main; }

struct Sculpt_manager::Main : Input_event_handler, Storage_dialog::Action
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

	Attached_rom_dataspace _block_devices_rom { _env, "block_devices" };

	Attached_rom_dataspace _usb_active_config_rom { _env, "usb_active_config" };

	Storage_devices _storage_devices { };

	Storage_dialog::Target _sculpt_partition { };

	Storage_dialog _storage_dialog { _env, _storage_devices, _sculpt_partition };

	void _handle_storage_devices_update();

	Signal_handler<Main> _storage_device_update_handler {
		_env.ep(), *this, &Main::_handle_storage_devices_update };

	Gui _gui { _env };

	Expanding_reporter _focus_reporter { _env, "focus", "focus" };

	Runtime _runtime { _env, _storage_devices };

	Signal_handler<Main> _runtime_state_handler {
		_env.ep(), *this, &Main::_handle_runtime_state };

	void _handle_runtime_state();

	/**
	 * Input_event_handler interface
	 */
	void handle_input_event(Input::Event const &ev) override
	{
		if (ev.key_press(Input::BTN_LEFT))
			_storage_dialog.click(*this);

		if (ev.key_release(Input::BTN_LEFT))
			_storage_dialog.clack(*this);
	}

	template <typename FN>
	void _apply_partition(Storage_dialog::Target const &target, FN const &fn)
	{
		_storage_devices.for_each_partition([&] (Partition_table::Label const &device,
		                                         Partition &partition) {
			if (device == target.device && partition.number == target.partition)
				fn(partition);
		});
	}

	/**
	 * Storage_dialog::Action interface
	 */
	void format(Storage_dialog::Target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.format_in_progress = true;
			_runtime.generate_config();
		});
	}

	void check(Storage_dialog::Target const &target) override
	{
		_apply_partition(target, [&] (Partition &partition) {
			partition.check_in_progress = true;
			_runtime.generate_config();
		});
	}

	void browse(Storage_dialog::Target const &target) override
	{
		log("browse_partition ", target.partition);
	}

	void use(Storage_dialog::Target const &target) override
	{
		_sculpt_partition = target;
		_runtime.generate_config();
	}

	Nitpicker::Root _gui_nitpicker { _env, _heap, *this };

	Main(Env &env) : _env(env)
	{
		_focus_reporter.generate([&] (Xml_generator &xml) {
//			xml.attribute("label", "manager -> input"); });
			xml.attribute("label", "wm -> "); });

		_runtime.state_sigh(_runtime_state_handler);

		_nitpicker.input()->sigh(_input_handler);

		_block_devices_rom.sigh    (_storage_device_update_handler);
		_usb_active_config_rom.sigh(_storage_device_update_handler);

		_handle_storage_devices_update();

		_runtime.generate_config();

		_storage_dialog.generate();

		_gui.generate_config();
	}
};


void Sculpt_manager::Main::_handle_storage_devices_update()
{
	bool reconfigure_runtime = false;
	{
		_block_devices_rom.update();
		Block_device_update_policy policy(_env, _heap, _storage_device_update_handler);
		_storage_devices.block_devices.update_from_xml(policy, _block_devices_rom.xml());

		_storage_devices.block_devices.for_each([&] (Block_device &dev) {

			dev.partition_table.process_part_blk_report();

			if (dev.partition_table.state == Partition_table::UNKNOWN) {
				reconfigure_runtime = true; };
		});
	}

	{
		_usb_active_config_rom.update();
		Usb_storage_device_update_policy policy(_env, _heap, _storage_device_update_handler);
		Xml_node const config = _usb_active_config_rom.xml();
		Xml_node const raw = config.has_sub_node("raw")
		                   ? config.sub_node("raw") : Xml_node("<raw/>");

		_storage_devices.usb_storage_devices.update_from_xml(policy, raw);

		_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device &dev) {

			dev.process_driver_report();
			dev.partition_table.process_part_blk_report();

			if (dev.partition_table.state == Partition_table::UNKNOWN) {
				reconfigure_runtime = true; };
		});
	}

	_storage_dialog.generate();

	if (reconfigure_runtime)
		_runtime.generate_config();
}


void Sculpt_manager::Main::_handle_runtime_state()
{
	_runtime._state.update();

	Xml_node state = _runtime._state.xml();
	log("runtime state update: ", state);

	bool reconfigure_runtime = false;

	/* check for completed file-system checks */
	_storage_devices.for_each_partition([&] (Partition_table::Label const &device,
	                                         Partition &partition) {
		if (partition.check_in_progress) {
			String<64> name(device, ".", partition.number, ".check");
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
			String<64> name(device, ".", partition.number, ".mkfs");
			Child_exit_state exit_state(state, name);

			if (exit_state.exited) {
				partition.format_in_progress = 0;
				partition.file_system.type = File_system::EXT2;
				reconfigure_runtime = true;
				_storage_dialog.reset_operation();
			}
		}
	});

	if (reconfigure_runtime)
		_runtime.generate_config();
}


void Component::construct(Genode::Env &env)
{
	static Sculpt_manager::Main main(env);
}

