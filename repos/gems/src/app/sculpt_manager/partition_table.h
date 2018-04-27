/*
 * \brief  Representation of partition table
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PARTITION_TABLE_H_
#define _PARTITION_TABLE_H_

#include "types.h"
#include "partition.h"
#include "xml.h"

namespace Sculpt_manager { struct Partition_table; };


struct Sculpt_manager::Partition_table
{
	enum State {
		UNKNOWN, /* partition information not yet known */
		USED,    /* part_blk is running and has reported partition info */
		RELEASED /* partition info is known but part_blk is not running */
	};

	Allocator &_alloc;

	typedef String<32> Label;

	Label const _label;

	State state { UNKNOWN };

	Partitions partitions { };

	Attached_rom_dataspace _partitions_rom;

	void process_part_blk_report()
	{
		_partitions_rom.update();
		log("partition info: ", _partitions_rom.xml());
		Partition_update_policy policy(_alloc);
		partitions.update_from_xml(policy, _partitions_rom.xml());
	}

	/**
	 * Constructor
	 *
	 * \param label  label of block device
	 * \param sigh   signal handler to be notified on partition-info updates
	 */
	Partition_table(Env &env, Allocator &alloc,
	                Label const &label, Signal_context_capability sigh)
	:
		_alloc(alloc), _label(label),
		_partitions_rom(env, String<80>("report -> runtime/", label, "/partitions").string())
	{
		_partitions_rom.sigh(sigh);
		process_part_blk_report();
	}

	bool part_blk_needed() const { return state == UNKNOWN || state == USED; }

	/**
	 * Generate content of start node for part_blk
	 *
	 * \param service_name  name of server that provides the block device, or
	 *                      if invalid, request block device from parent.
	 */
	inline void gen_part_blk_start_content(Xml_generator &xml, Label const &server_name) const;
};


void Sculpt_manager::Partition_table::gen_part_blk_start_content(Xml_generator &xml,
                                                                 Label const &server_name) const
{
	gen_common_start_content(xml, _label,
	                         Cap_quota{100}, Ram_quota{8*1024*1024});

	xml.node("binary", [&] () { xml.attribute("name", "part_blk"); });

	xml.node("config", [&] () {
		xml.node("report", [&] () { xml.attribute("partitions", "yes"); });

		for (unsigned i = 1; i < 10; i++) {
			xml.node("policy", [&] () {
				xml.attribute("label",     i);
				xml.attribute("partition", i);
				xml.attribute("writeable", "yes");
			});
		}
	});

	xml.node("provides", [&] () {
		xml.node("service", [&] () {
			xml.attribute("name", Block::Session::service_name()); }); });

	xml.node("route", [&] () {
		gen_parent_unscoped_rom_route(xml, "part_blk");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session> (xml);
		gen_parent_route<Pd_session>  (xml);
		gen_parent_route<Log_session> (xml);

		xml.node("service", [&] () {
			xml.attribute("name", Block::Session::service_name());
			if (server_name.valid()) {
				xml.node("child", [&] () {
					xml.attribute("name", server_name); });
			} else {
				xml.node("parent", [&] () {
					xml.attribute("label", _label); });
			}
		});
		xml.node("service", [&] () {
			xml.attribute("name", Report::Session::service_name());
			xml.attribute("label", "partitions");
			xml.node("parent", [&] () { });
		});
	});
}

#endif /* _PARTITION_TABLE_H_ */
