/*
 * \brief  Utility to sequence component execution
 * \author Emery Hemingway
 * \date   2017-08-09
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/debug.h>

#include <file_system_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>


namespace Fs_replay {
	using namespace Genode;
	using namespace File_system;

	struct Main;

	static Xml_node trace_config(Xml_node config)
	{
		try { return config.sub_node("trace"); }
		catch (Xml_node::Nonexistent_sub_node) {
			return Xml_node("trace"); }
	}
}


struct Fs_replay::Main
{
	Genode::Env &env;

	Attached_rom_dataspace trace_rom;

	Heap heap { env.ram(), env.rm() };

	Allocator_avl packet_alloc { &heap };

	File_system::Connection fs;

	Main(Genode::Env &env, Xml_node config)
	:
		env(env),
		trace_rom(env, config.attribute_value("rom", String<64>("trace.log")).string()),
		fs(env, packet_alloc, "",
		   config.attribute_value("root", String<MAX_PATH_LEN/2>("")).string())
	{ }

	void replay();
};


void Fs_replay::Main::replay()
{
}


void Component::construct(Genode::Env &env)
{
	Genode::Attached_rom_dataspace config_rom(env, "config");

	Fs_replay::Main main(env, Fs_replay::trace_config(config_rom.xml()));

	main.replay();

	env.parent().exit(0);
}
