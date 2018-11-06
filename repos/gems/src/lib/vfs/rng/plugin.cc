/*
 * \brief  Rng service random file system
 * \author Emery Hemingway
 * \date   2018-11-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vfs/file_system_factory.h>
#include <vfs/single_file_system.h>
#include <rng_session/connection.h>

namespace Vfs {
	class Rng_file_system;
	struct Rng_factory;
}

class Vfs::Rng_file_system : public Vfs::Single_file_system
{
	private:

		Genode::Env &_env;

		class Rng_vfs_handle : public Single_vfs_handle
		{
			private:

				Rng::Connection _rng;

			public:

				Rng_vfs_handle(Genode::Env &env,
				                         Directory_service &ds,
				                         File_io_service   &fs,
				                         Genode::Allocator &alloc)
				: Single_vfs_handle(ds, fs, alloc, 0), _rng(env) { }

				Read_result read(char *dst, Vfs::file_size count,
				                 Vfs::file_size &out_count) override
				{
					using Genode::uint64_t;
					using Genode::size_t;

					for (size_t i = 0; i < count; i += sizeof(uint64_t)) {
						uint64_t entropy = _rng.gather();
						Genode::memcpy(dst+i, &entropy, sizeof(uint64_t));
					}

					if (count % sizeof(uint64_t)) {
						uint64_t entropy = _rng.gather();
						char const *src = (char*)&entropy;
						for (size_t i = count % sizeof(uint64_t); i < count; ++ i)
							dst[i] = src[sizeof(uint64_t)%i];
					}

					out_count = count;

					return READ_OK;
				}

				Write_result write(char const *, Vfs::file_size,
				                   Vfs::file_size &) override
				{
					return WRITE_ERR_IO;
				}

				bool read_ready() { return true; }
		};

	public:

		Rng_file_system(Vfs::Env &env, Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_env(env.env())
		{ }

		static char const *name()   { return "rng"; }
		char const *type() override { return "rng"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const         *path, unsigned,
		                 Vfs::Vfs_handle   **out_handle,
		                 Genode::Allocator  &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle = new (alloc)
				Rng_vfs_handle(_env, *this, *this, alloc);
			return OPEN_OK;
		}

};


struct Vfs::Rng_factory : Vfs::File_system_factory
{
	Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
	{
		return new (env.alloc()) Rng_file_system(env, config);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Vfs::Rng_factory factory;
	return &factory;
}
