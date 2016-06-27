/*
 * \brief  VFS File_system server
 * \author Emery Hemingway
 * \date   2015-08-16
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/sleep.h>

/* Local includes */
#include "fs_component.h"
#include "rom_component.h"
#include "report_component.h"

namespace Vfs_server { struct Main; }


struct Vfs_server::Main
{
	Genode::Env &env;

	Genode::Attached_rom_dataspace config_rom { env, "config" };
	Genode::Xml_node config_node = config_rom.xml();

	Genode::Sliced_heap sliced_heap { &env.ram(), &env.rm() };
	Genode::Heap               heap { &env.ram(), &env.rm() };

	void update_config() { config_rom.update(); }

	Genode::Signal_handler<Main> _config_handler
		{ env.ep(), *this, &Main::update_config };

	Genode::Xml_node vfs_config()
	{
		try { return config_rom.xml().sub_node("vfs"); }
		catch (...) {
			Genode::error("vfs not configured");
			env.parent().exit(~0);
			Genode::sleep_forever();
		}
	}

	Vfs::Dir_file_system vfs_root
		{ vfs_config(), Vfs::global_file_system_factory() };

	Fs_root         *fs_root;
	Rom_root       *rom_root;
	Report_root *report_root;

	Main(Genode::Env &env) : env(env)
	{
		Genode::Xml_node config_node = config_rom.xml();

		bool     fs_enabled = config_node.has_sub_node("file_system");
		bool    rom_enabled = config_node.has_sub_node("rom");
		bool report_enabled = config_node.has_sub_node("report");

		if (!fs_enabled)
			Genode::warning("no 'file_system' node found in config, "
			                "enabling service anyway, this behaviour "
			                "will not peristent indefinitly");
		{
			fs_root = new (heap)
				Fs_root(env, sliced_heap, vfs_root, config_rom);
			env.parent().announce(env.ep().manage(*fs_root));
		}

		if (rom_enabled) {
			rom_root = new (heap)
				Rom_root(env, sliced_heap, vfs_root, config_rom);
			env.parent().announce(env.ep().manage(*rom_root));
		} else
			Genode::warning("ROM service not enabled");

		if (report_enabled) {
			report_root = new (heap)
				Report_root(env, sliced_heap, vfs_root, config_rom);
			env.parent().announce(env.ep().manage(*report_root));
		} else
			Genode::warning("Report service not enabled");

		/*
		 * XXX: when the FS service is not enabled by default
		 * if (!(fs_enabled || rom_enabled || report_enabled)) {
		 * 	Genode::error("no services enabled, exiting");
		 * 	env.parent().exit(~0);
		 * }
		 */
	}
};


/***************
 ** Component **
 ***************/

Genode::size_t Component::stack_size() {
	return 8*1024*sizeof(long); }

void Component::construct(Genode::Env &env) {
	static Vfs_server::Main server(env); }
