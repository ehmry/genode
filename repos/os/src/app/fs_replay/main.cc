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
#include <log_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <util/print_lines.h>


namespace Fs_replay {
	using namespace Genode;
	using namespace File_system;

	template <typename HANDLE_TYPE>
	struct Open_handle
	{
		HANDLE_TYPE const handle;
		Id_space<Open_handle<HANDLE_TYPE>>::Element space_elem;

		Open_handle(HANDLE_TYPE &h, Id_space<Open_handle<HANDLE_TYPE>> &space)
		: handle(h), space_elem(*this, h.id, space) { }
	};

	/*
	struct Open_file
	{
	};

	struct Open_symlink
	{
	};

	typedef Genode::Id_space<Open_dir>     Directory_space;
	typedef Genode::Id_space<Open_file>    File_space;
	typedef Genode::Id_space<Open_symlink> Symlink_space;
	*/

	struct Main;

	static Xml_node trace_config(Xml_node config)
	{
		try { return config.sub_node("trace"); }
		catch (Xml_node::Nonexistent_sub_node) {
			return Xml_node("<trace/>"); }
	}
}


struct Fs_replay::Main
{
	Genode::Env &env;

	Attached_rom_dataspace trace_rom;

	Heap heap { env.ram(), env.rm() };

	Allocator_avl packet_alloc { &heap };

	File_system::Connection fs;

/*
	Directory_space dir_space;
	File_space      file_space;
	Symlink_space   symlink_space;
 */

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
	auto line_func = [&] (char const *line)
	{
		if (MATCH_VERB("cl", line)) { } else
		if (MATCH_VERB("dh", line)) { } else
		if (MATCH_VERB("do", line)) { } else
		if (MATCH_VERB("fh", line)) { } else
		if (MATCH_VERB("fo", line)) { } else
		if (MATCH_VERB("nh", line)) { } else
		if (MATCH_VERB("no", line)) { } else
		if (MATCH_VERB("re", line)) { } else
		if (MATCH_VERB("rn", line)) { } else
		if (MATCH_VERB("sh", line)) { } else
		if (MATCH_VERB("sl", line)) { } else
		if (MATCH_VERB("st", line)) { } else
		if (MATCH_VERB("sy", line)) { } else
		if (MATCH_VERB("tr", line)) { } else
		if (MATCH_VERB("us", line)) { } else
		if (MATCH_VERB("wr", line)) { } else
		{ }
};

	print_lines<Log_session::MAX_STRING_LEN>(
		trace_rom.local_addr<char const>(), trace_rom.size(), line_func);
}


void Component::construct(Genode::Env &env)
{
	Genode::Attached_rom_dataspace config_rom(env, "config");

	Fs_replay::Main main(env, Fs_replay::trace_config(config_rom.xml()));

	main.replay();

	env.parent().exit(0);
}
