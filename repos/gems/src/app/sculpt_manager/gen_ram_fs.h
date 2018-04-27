/*
 * \brief  XML configuration for RAM file system
 * \author Norman Feske
 * \date   2018-05-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GEN_RAM_FS_H_
#define _GEN_RAM_FS_H_

#include "xml.h"
#include "ram_fs_state.h"

namespace Sculpt_manager {

	void gen_ram_fs_start_content(Xml_generator &, Ram_fs_state const &);
}


void Sculpt_manager::gen_ram_fs_start_content(Xml_generator &xml,
                                              Ram_fs_state const &state)
{
	xml.attribute("version", state.version.value);

	gen_common_start_content(xml, "ram_fs", Cap_quota{300}, state.ram_quota);

	gen_provides<::File_system::Session>(xml);

	xml.node("config", [&] () {
		xml.node("default-policy", [&] () {
			xml.attribute("root",      "/");
			xml.attribute("writeable", "yes");
		});
	});

	xml.node("route", [&] () {
		gen_parent_rom_route(xml, "ram_fs");
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session> (xml);
		gen_parent_route<Pd_session>  (xml);
		gen_parent_route<Log_session> (xml);
	});
}

#endif /* _GEN_RAM_FS_H_ */
