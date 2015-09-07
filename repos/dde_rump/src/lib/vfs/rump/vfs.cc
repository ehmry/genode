/*
 * \brief  Rump VFS plugin
 * \author Emery Hemingway
 * \date   2015-09-07
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>
#include <rump_fs/fs.h>
#include <timer_session/connection.h>
#include <base/lock.h>

/* local includes */
#include "vfs_rump.h"

extern "C" {
#include <rump/rump.h>
}


class Rump_factory : public Vfs::File_system_factory
{
	private:

		Genode::Lock _lock;

		class Sync : public Genode::Thread<1024 * sizeof(Genode::addr_t)>
		{
			private:

				Timer::Connection _timer;
				Genode::Lock     &_lock;

			protected:

				void entry()
				{
					for (;;) {
						_timer.msleep(1000);
						_lock.lock();

						/* sync through front-end */
						rump_sys_sync();

						/* sync Genode back-end */
						rump_io_backend_sync();
						_lock.unlock();
					}
				}

				public:

					Sync(Genode::Lock &lock)
					: Thread("vfs_rump_sync"), _lock(lock) { }
		} _sync;

	public:

		Rump_factory()
		: _sync(_lock)
		{
			/* start rump kernel */
			rump_init();

			/* register block device */
			rump_pub_etfs_register(
				GENODE_DEVICE, GENODE_BLOCK_SESSION, RUMP_ETFS_BLK);

			/* set all bits but the stickies */
			rump_sys_umask(S_ISUID|S_ISGID|S_ISVTX);

			_sync.start();
		}

		Vfs::File_system *create(Genode::Xml_node node) override
		{
			return new (Genode::env()->heap()) Vfs::Rump_file_system(_lock, node);
		}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Rump_factory factory;
	return &factory;
}
