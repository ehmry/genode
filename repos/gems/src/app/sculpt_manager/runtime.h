/*
 * \brief  Sculpt runtime management
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RUNTIME_H_
#define _RUNTIME_H_

/* Genode includes */
#include <os/reporter.h>

/* local includes */
#include "types.h"
#include "storage_devices.h"

namespace Sculpt_manager { struct Runtime; }

namespace Sculpt_manager {

	void gen_fsck_start_content(Xml_generator &xml, Block_device::Label const &,
	                            Partition::Number const &);
	void gen_mkfs_start_content(Xml_generator &xml, Block_device::Label const &,
	                            Partition::Number const &);
}

struct Sculpt_manager::Runtime
{
	Env &_env;

	Storage_devices const &_storage_devices;

	Attached_rom_dataspace _state { _env, "runtime_state" };

	Expanding_reporter _config { _env, "config", "runtime_config" };

	inline void _generate_config(Xml_generator &) const;

	void generate_config()
	{
		_config.generate([&] (Xml_generator &xml) { _generate_config(xml); });
	}

	void state_sigh(Signal_context_capability sigh) { _state.sigh(sigh); }

	Runtime(Env &env, Storage_devices const &storage_devices)
	: _env(env), _storage_devices(storage_devices) { }
};


void Sculpt_manager::Runtime::_generate_config(Xml_generator &xml) const
{
	xml.attribute("verbose", "yes");

	xml.node("report", [&] () {
		xml.attribute("delay_ms", 500); });

	xml.node("parent-provides", [&] () {
		gen_parent_service<Rom_session>(xml);
		gen_parent_service<Cpu_session>(xml);
		gen_parent_service<Pd_session>(xml);
		gen_parent_service<Log_session>(xml);
		gen_parent_service<Timer::Session>(xml);
		gen_parent_service<Report::Session>(xml);
		gen_parent_service<Platform::Session>(xml);
		gen_parent_service<Block::Session>(xml);
		gen_parent_service<Usb::Session>(xml);
		gen_parent_service<::File_system::Session>(xml);
	});

	_storage_devices.block_devices.for_each([&] (Block_device const &dev) {
		if (dev.partition_table.part_blk_needed())
			xml.node("start", [&] () {
				Partition_table::Label const parent { };
				dev.partition_table.gen_part_blk_start_content(xml, parent); }); });

	_storage_devices.usb_storage_devices.for_each([&] (Usb_storage_device const &dev) {

		if (dev.usb_block_drv_needed())
			xml.node("start", [&] () {
				dev.gen_usb_block_drv_start_content(xml);
		});

		if (dev.partition_table.part_blk_needed())
			xml.node("start", [&] () {
				Partition_table::Label const driver = dev.usb_block_drv_name();
				dev.partition_table.gen_part_blk_start_content(xml, driver);
		});
	});

	_storage_devices.for_each_partition([&] (Partition_table::Label const &device,
	                                         Partition const &partition) {
		if (partition.check_in_progress)
			xml.node("start", [&] () {
				gen_fsck_start_content(xml, device, partition.number); });

		if (partition.format_in_progress)
			xml.node("start", [&] () {
				gen_mkfs_start_content(xml, device, partition.number); });
	});
}

#endif /* _RUNTIME_H_ */
