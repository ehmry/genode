/*
 * \brief  Report filesystem
 * \author Emery Hemingway
 * \date   2017-04-29
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS__REPORT_FILE_SYSTEM_H_
#define _VFS__REPORT_FILE_SYSTEM_H_

#include <report_session/connection.h>
#include <base/attached_dataspace.h>
#include <vfs/single_file_system.h>
#include <vfs/file_system.h>

namespace Vfs { class Report_file_system; }


class Vfs::Report_file_system : public Single_file_system
{
	private:

		typedef Genode::String<64> Label;

		Label label(Xml_node const &config)
		{
			Label label;

			/* obtain label from config */
			try { config.attribute("label").value(&label); }

			/* use VFS node name if label was not provided */
			catch (Genode::Xml_node::Nonexistent_attribute) {
				config.attribute("name").value(&label); }

			return label;
		}

		Report::Connection         _report;
		Genode::Attached_dataspace _ds;
		file_size                  _length = 0;
		bool                       _dirty = false;

	public:

		Report_file_system(Genode::Env &env,
		                  Genode::Allocator&,
		                  Genode::Xml_node config,
		                  Io_response_handler &)
		:
			Single_file_system(NODE_TYPE_FILE, name(), config),
			_report(env, label(config).string()),
			_ds(env.rm(), _report.dataspace())
		{ }

		static char const *name()   { return "report"; }
		char const *type() override { return "report"; }


		/*********************************
		 ** Directory-service interface **
		 ********************************/

		Dataspace_capability dataspace(char const *path) override
		{
			return _ds.cap();
		}

		void sync(char const *path) override
		{
			if (_dirty)
				_report.submit(_length);
		}

		void close(Vfs_handle *handle) override
		{
			if (_dirty)
				_report.submit(_length);
			Single_file_system::close(handle);
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			out.size = _length;
			return result;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *handle, char const *src, file_size count,
		                   file_size &out) override
		{
			using namespace Genode;

			file_size seek = handle->seek();
			file_size max = _ds.size();
			if (seek >= max) {
				out = 0;
				Genode::error("Report file dataspace exhausted");
				return WRITE_ERR_INVALID;
			} else {
				file_size n = min(max-seek, count);
				Genode::memcpy(_ds.local_addr<char>() + seek, src, n);
				if (n > _length)
					_length = n;
				_dirty = true;
				out = n;
				return WRITE_OK;
			}
		}

		Read_result read(Vfs_handle *handle, char *dst, file_size count,
		                 file_size &out) override
		{
			using namespace Genode;

			file_size seek = handle->seek();
			if (seek > _length) {
				out = 0;
				return READ_ERR_INVALID;
			} else {
				file_size n = min(_length-seek, count);
				Genode::memcpy(dst, _ds.local_addr<char const>() + seek, n);
				out = n;
				return READ_OK;
			}
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size length) override
		{
			if (length > _ds.size())
				return FTRUNCATE_ERR_NO_PERM;
			if (length < _length)
				Genode::memset(
					_ds.local_addr<char>() + length,
					0x00, _length - length);
			_length = length;
			return FTRUNCATE_OK;
		}

		bool read_ready(Vfs_handle *) override { return true; }
};

#endif /* _VFS__REPORT_FILE_SYSTEM_H_ */
