/*
 * \brief  XML configuration for formatting a file system
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "xml.h"
#include "runtime.h"

void Sculpt_manager::gen_mkfs_start_content(Xml_generator &xml,
                                            Block_device::Label const &device,
                                            Partition::Number const &partition)
{
	gen_common_start_content(xml, String<64>(device, ".", partition, ".mkfs"),
	                         Cap_quota{500}, Ram_quota{100*1024*1024});

	xml.node("binary", [&] () { xml.attribute("name", "noux"); });

	xml.node("config", [&] () {
		xml.attribute("stdout", "/dev/log");
		xml.attribute("stderr", "/dev/log");
		xml.attribute("stdin",  "/dev/null");
		xml.node("fstab", [&] () {
			xml.node("tar", [&] () {
				xml.attribute("name", "e2fsprogs-minimal.tar"); });
			xml.node("dir", [&] () {
				xml.attribute("name", "dev");
				xml.node("block", [&] () {
					xml.attribute("name", "block");
					xml.attribute("label", "default");
					xml.attribute("block_buffer_count", 128);
				});
				xml.node("null", [&] () {});
				xml.node("log",  [&] () {});
			});
		});
		xml.node("start", [&] () {
			xml.attribute("name", "/bin/mkfs.ext2");
			xml.node("arg", [&] () { xml.attribute("value", "/dev/block"); });
		});
	});

	xml.node("route", [&] () {
		gen_parent_unscoped_rom_route(xml, "noux");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Rom_session>    (xml);
		gen_parent_route<Timer::Session> (xml);

		xml.node("service", [&] () {
			xml.attribute("name", Block::Session::service_name());
			xml.node("child", [&] () {
				xml.attribute("name",  device);
				xml.attribute("label", partition);
			});
		});
	});
}
