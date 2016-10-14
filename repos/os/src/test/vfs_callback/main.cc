/*
 * \brief  VFS notify callback cascade test
 * \author Emery Hemingway
 * \date   2016-05-10
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>


using namespace Genode;

typedef Genode::Path<256> Path;

struct Callback_test : Vfs::Notify_callback
{
	Genode::Heap         &heap;
	::Path                  cur_full_path;
	char           const *cur_leaf_path;
	Vfs::Vfs_handle      *cur_handle;
	Callback_test        *next_test = nullptr;
	unsigned             &depth;

	Callback_test(Vfs::Dir_file_system &vfs,
	              Genode::Heap         &heap,
	              char           const *path,
	              Vfs::Vfs_handle      *handle,
	              unsigned             &depth)
	:
		heap(heap),
		cur_full_path(path),
		cur_leaf_path(vfs.leaf_path(cur_full_path.base())),
		cur_handle(handle),
		depth(depth)
	{
		::Path next_path(path);
		next_path.strip_last_element();
		next_path.append("/d");

		if (vfs.mkdir(next_path.base(), 0) != Vfs::Directory_service::Mkdir_result::MKDIR_OK) {
			error("failed to create directory `",next_path,"'");
			return;
		}
		next_path.append("/f");

		Vfs::Vfs_handle *next_handle = nullptr;

		if (vfs.open(next_path.base(),
		         Vfs::Directory_service::OPEN_MODE_RDWR |
		         Vfs::Directory_service::OPEN_MODE_CREATE,
		         &next_handle, heap) != Vfs::Directory_service::Open_result::OPEN_OK)
		{
			error("failed to create file `",next_path,"'");
			return;
		}

		/* set callback */
		next_handle->notify_callback(*this);
		if (next_handle->ds().subscribe(next_handle))
			log("callback -> ", path);
		else {
			error("callback not supported at ", path);
			return;
		}


		if (--depth == 0) {
			/* trigger callback cascade */
			Vfs::file_size out;
			char const data = 0;
			next_handle->fs().write(cur_handle, &data, 1, out);
			next_handle->ds().sync(cur_leaf_path);
			++depth;
		} else {
			next_test = new (heap)
				Callback_test(vfs, heap, next_path.base(), next_handle, depth);
		}
	}

	~Callback_test()
	{
		Vfs::Directory_service &ds = cur_handle->ds();
		ds.close(cur_handle);
		ds.unlink(cur_leaf_path);

		if (next_test)
			destroy(heap, next_test);
	}

	void notify() override
	{
		log("callback <- ", cur_full_path);
		++depth;

		Vfs::file_size out;
		char const data = 0;
		cur_handle->fs().write(cur_handle, &data, 1, out);
		cur_handle->ds().sync(cur_leaf_path);
	}
};


struct Main
{
	Genode::Env &env;

	Attached_rom_dataspace config_rom { env, "config" };

	Timer::Connection timer { env };

	Heap heap { env.ram(), env.rm() };

	Vfs::Dir_file_system vfs
		{ env, heap,
		  config_rom.xml().sub_node("vfs"),
		  Vfs::global_file_system_factory()
		};

	Xml_node top_dir_node = config_rom.xml().sub_node("vfs").sub_node("dir");

	Callback_test *curr_test = nullptr;

	/*
	 * Decrement a counter as files are created and
	 * increment it when callbacks are issued.
	 */
	enum { TEST_DEPTH = 8 };
	unsigned depth = TEST_DEPTH;
	unsigned sleep_count = 0;
	int failed_tests = 0;

	void start_test()
	{
		typedef Genode::String<16> Dir_name;
		Dir_name dir_name = top_dir_node.attribute_value("name", Dir_name());

		::Path root_path(dir_name.string(), "/");
		log("testing ", root_path);

		Vfs::Vfs_handle *root_handle = nullptr;
		root_path.append("/f");
		vfs.open(root_path.base(),
		         Vfs::Directory_service::OPEN_MODE_RDWR |
		         Vfs::Directory_service::OPEN_MODE_CREATE,
		         &root_handle, heap);

		depth = TEST_DEPTH;
		curr_test = new (heap)
			Callback_test(vfs, heap, root_path.base(), root_handle, depth);

		sleep_count = 0;
		timer.trigger_once(100);
	}

	void handle_timeout()
	{
		if (depth != TEST_DEPTH) {
			if (++sleep_count < TEST_DEPTH) {
				warning("sleeping for callback");
				timer.trigger_once(100);
				vfs.sync("/");
				return;
			} else {
				error("callback test failed"); /* for ", top_dir_node);*/
				++failed_tests;
			}
		}

		destroy(heap, curr_test);
		try {
			do {
				top_dir_node = top_dir_node.next();
			} while (!top_dir_node.has_type("dir"));
		} catch (Xml_node::Nonexistent_sub_node) {
			env.parent().exit(failed_tests);
			return;
		}
		start_test();
	}

	Signal_handler<Main> timeout_handler
		{ env.ep(), *this, &Main::handle_timeout };

	Main(Genode::Env &env) : env(env)
	{
		timer.sigh(timeout_handler);
		start_test();
	}

};


Genode::size_t Component::stack_size()      { return 8*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Main inst(env); }
