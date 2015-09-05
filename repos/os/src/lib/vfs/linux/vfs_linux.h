/*
 * \brief  Linux host VFS plugin
 * \author Emery Hemingway
 * \date   2015-09-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef __VFS_LINUX__VFS_LINUX_H_
#define __VFS_LINUX__VFS_LINUX_H_

/* Genode includes */
#include <vfs/file_system.h>
#include <vfs/vfs_handle.h>
#include <util/xml_node.h>

/* Linux includes */
extern "C" {
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}

using namespace Vfs;

class Linux_file_system : public Vfs::File_system
{
	private:

		class Linux_vfs_handle : public Vfs::Vfs_handle
		{
			private:

				int _fd;

			public:

				Linux_vfs_handle(File_system &fs, int status_flags, int fd)
				: Vfs_handle(fs, fs, status_flags), _fd(fd) { }

				~Linux_vfs_handle() { ::close(_fd); }

				int fd() const { return _fd; }
		};

		Genode::Lock _lock;
		char         _path[Vfs::MAX_PATH_LEN];
		char        *_path_start;
		size_t       _subpath_len;

	public:

		Linux_file_system(Genode::Xml_node config)
		: _subpath_len(Vfs::MAX_PATH_LEN)
		{
			try {
				config.attribute("root").value(_path, sizeof(_path));
				_path_start = &_path[0];
				while (*_path_start) {
					++_path_start;
					--_subpath_len;
				}
			} catch (...) {
				strncpy(_path, ".", 2);
				_path_start = &_path[1];
				--_subpath_len;
			}
		}

		/***************************
		 ** File_system interface **
		 ***************************/

		static char const *name() { return "linux"; }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Genode::Dataspace_capability dataspace(char const *path) override
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			int fd = ::open(_path, O_RDONLY);
			if (fd == -1) return Genode::Dataspace_capability();

			struct stat s;
			if (lstat(_path, &s) != 0) return Genode::Dataspace_capability();

			Ram_dataspace_capability ds_cap;
			char *local_addr = nullptr;
			try {
				ds_cap = env()->ram_session()->alloc(s.st_size);

				local_addr = env()->rm_session()->attach(ds_cap);

				ssize_t n = ::read(fd, local_addr, s.st_size);
				env()->rm_session()->detach(local_addr);
				if (n != s.st_size) {
					env()->ram_session()->free(ds_cap);
				}
			} catch(...) {
				env()->rm_session()->detach(local_addr);
				env()->ram_session()->free(ds_cap);
			}
			::close(fd);
			return ds_cap;
		}

		void release(char const *path,
		             Genode::Dataspace_capability ds_cap) override
		{
			env()->ram_session()->free(static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		file_size num_dirent(char const *path)
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			DIR *dirp = ::opendir(_path);
			if (!dirp) return 0;

			file_size count = 0;
			while (::readdir(dirp))
				++count;

			::closedir(dirp);
			return count;
		}

		bool is_directory(char const *path)
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			struct stat s;
			if (::lstat(_path, &s) != 0) return false;
			return S_ISDIR(s.st_mode);
		}

		char const *leaf_path(char const *path)
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			struct stat s;
			return (::lstat(_path, &s) == 0) ? path : 0;
		}

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			if (::mkdir(_path, mode) != 0) switch (errno) {
			case ENAMETOOLONG: return MKDIR_ERR_NAME_TOO_LONG;
			case EACCES:       return MKDIR_ERR_NO_PERM;
			case ENOENT:       return MKDIR_ERR_NO_ENTRY;
			case EEXIST:       return MKDIR_ERR_EXISTS;
			case ENOSPC:       return MKDIR_ERR_NO_SPACE;
			default:
				return MKDIR_ERR_NO_PERM;
			}
			return MKDIR_OK;
		}

		Open_result open(char const *path, unsigned mode,
		                 Vfs_handle **handle) override
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			int fd = ::open(_path, mode);
			if (fd == -1) switch (errno) {
			case ENAMETOOLONG: return OPEN_ERR_NAME_TOO_LONG;
			case EACCES:       return OPEN_ERR_NO_PERM;
			case ENOENT:       return OPEN_ERR_UNACCESSIBLE;
			case EEXIST:       return OPEN_ERR_EXISTS;
			case ENOSPC:       return OPEN_ERR_NO_SPACE;
			default:
				return OPEN_ERR_NO_PERM;
			}

			*handle = new (env()->heap()) Linux_vfs_handle(*this, mode, fd);
			return OPEN_OK;
		}

		Stat_result stat(char const *path, Stat &stat)
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			struct ::stat sb;
			if (::lstat(_path, &sb) != 0) return STAT_ERR_NO_ENTRY;

			stat.size = sb.st_size;
			stat.mode = sb.st_mode;
			stat.uid  = sb.st_uid;
			stat.gid  = sb.st_gid;
			stat.inode = sb.st_ino;
			stat.device = sb.st_dev;

			return STAT_OK;
		}


		Dirent_result dirent(char const *path, file_offset index,
		                     Dirent &vfs_dir) override
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			memset(&vfs_dir, 0x00, sizeof(Dirent));
			struct ::dirent *lx_dir = nullptr;

			DIR *dirp = ::opendir(_path);
			if (!dirp) return DIRENT_ERR_INVALID_PATH;
			::seekdir(dirp, index);
			lx_dir = ::readdir(dirp);

			if (!lx_dir) {
				vfs_dir.fileno = index+1;
				vfs_dir.type = DIRENT_TYPE_END;
				closedir(dirp);
				return DIRENT_OK;
			}
			vfs_dir.fileno = index+1;
			switch (lx_dir->d_type) {
			case DT_REG:  vfs_dir.type = DIRENT_TYPE_FILE;      break;
			case DT_DIR:  vfs_dir.type = DIRENT_TYPE_DIRECTORY; break;
			case DT_FIFO: vfs_dir.type = DIRENT_TYPE_FIFO;      break;
			case DT_CHR:  vfs_dir.type = DIRENT_TYPE_CHARDEV;   break;
			case DT_BLK:  vfs_dir.type = DIRENT_TYPE_BLOCKDEV;  break;
			case DT_LNK:  vfs_dir.type = DIRENT_TYPE_SYMLINK;   break;
			}

			strncpy(vfs_dir.name, lx_dir->d_name, 4);
			closedir(dirp);
			return DIRENT_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			if (::unlink(_path) == 0) return UNLINK_OK;
			if (errno == ENOENT)   return UNLINK_ERR_NO_ENTRY;
			return UNLINK_ERR_NO_PERM;
		}

		Readlink_result readlink(char const *path, char *buf,
		                         file_size buf_size, file_size &out_len) override
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, path, _subpath_len);

			if (::readlink(_path, buf, buf_size) != 0) {
				out_len = strlen(buf);
				return READLINK_OK;
			}
			out_len = 0;
			return READLINK_ERR_NO_ENTRY;
		}

		Symlink_result symlink(char const *from, char const *to) override
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, to, _subpath_len);

			if (::symlink(from, _path) != 0) switch (errno) {
			case EEXIST:       return SYMLINK_ERR_EXISTS;
			case ENOENT:       return SYMLINK_ERR_NO_ENTRY;
			case ENOSPC:       return SYMLINK_ERR_NO_SPACE;
			case EACCES:       return SYMLINK_ERR_NO_PERM;
			case ENAMETOOLONG: return SYMLINK_ERR_NAME_TOO_LONG;
			default:
				return SYMLINK_ERR_NO_PERM;
			}
			return SYMLINK_OK;
		}

		Rename_result rename(char const *from, char const *to) override
		{
			Genode::Lock::Guard guard(_lock);
			strncpy(_path_start, from, _subpath_len);

			size_t len = Vfs::MAX_PATH_LEN - _subpath_len;
			char _to[Vfs::MAX_PATH_LEN];
			strncpy(_to, _path, len+1);
			strncpy(_to+len, to, _subpath_len);

			if (::rename(_path, _to) != 0) switch (errno) {
			case ENOENT: return RENAME_ERR_NO_ENTRY;
			case EXDEV:  return RENAME_ERR_CROSS_FS;
			case EACCES: return RENAME_ERR_NO_PERM;
			}
			return RENAME_OK;
		}


		/*******************************
		 ** File io service interface **
		 *******************************/

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *buf, file_size buf_size,
		                   file_size &out_count) override
		{
			Linux_vfs_handle *handle = static_cast<Linux_vfs_handle *>(vfs_handle);
			ssize_t n = ::write(handle->fd(), buf, buf_size);
			if (n == -1) switch (errno) {
			case EWOULDBLOCK: return WRITE_ERR_WOULD_BLOCK;
			case EINVAL:      return WRITE_ERR_INVALID;
			case EIO:         return WRITE_ERR_IO;
			case EINTR:       return WRITE_ERR_INTERRUPT;
			default:
				return WRITE_ERR_IO;
			}
			out_count = n;
			return WRITE_OK;
		}

		Read_result read(Vfs_handle *vfs_handle, char *buf, file_size buf_size,
		                 file_size &out_count) override
		{
			Linux_vfs_handle *handle = static_cast<Linux_vfs_handle *>(vfs_handle);
			ssize_t n = ::read(handle->fd(), buf, buf_size);
			if (n == -1) switch (errno) {
			case EWOULDBLOCK: return READ_ERR_WOULD_BLOCK;
			case EINVAL:      return READ_ERR_INVALID;
			case EIO:         return READ_ERR_IO;
			case EINTR:       return READ_ERR_INTERRUPT;
			default:
				return READ_ERR_IO;
			}
			out_count = n;
			return READ_OK;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size len) override
		{
			Linux_vfs_handle *handle = static_cast<Linux_vfs_handle *>(vfs_handle);

			if (::ftruncate(handle->fd(), len) != 0) switch (errno) {
			case EACCES: return FTRUNCATE_ERR_NO_PERM;
			case EINTR:  return FTRUNCATE_ERR_INTERRUPT;
			case ENOSPC: return FTRUNCATE_ERR_NO_SPACE;
			default:
				return FTRUNCATE_ERR_NO_PERM;
			}
			return FTRUNCATE_OK;
		}
};

#endif
