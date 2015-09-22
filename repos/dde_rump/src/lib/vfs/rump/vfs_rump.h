/*
 * \brief  Rump VFS plugin
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Emery Hemingway
 * \date   2015-09-05
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/file_system.h>
#include <vfs/vfs_handle.h>
#include <rump_fs/fs.h>
#include <os/path.h>
#include <util/xml_node.h>

extern "C" {
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <rump/rump.h>
#include <rump/rump_syscalls.h>
}

extern int errno;

namespace Vfs { struct Rump_file_system; };

static char const *fs_types[] = { RUMP_MOUNT_CD9660, RUMP_MOUNT_EXT2FS,
                                  RUMP_MOUNT_FFS, RUMP_MOUNT_MSDOS,
                                  RUMP_MOUNT_NTFS, RUMP_MOUNT_UDF, 0 };

class Vfs::Rump_file_system : public File_system
{
	private:

		enum { BUFFER_SIZE = 4096 };

		typedef Genode::Path<MAX_PATH_LEN> Path;

		Genode::Lock &_lock;

		class Rump_vfs_handle : public Vfs_handle
		{
			private:

				int _fd;

			public:

				Rump_vfs_handle(File_system &fs, int status_flags, int fd)
				: Vfs_handle(fs, fs, status_flags), _fd(fd) { }

				~Rump_vfs_handle() { rump_sys_close(_fd); }

				int fd() const { return _fd; }
		};

		/**
		 * We define our own fs arg structure to fit all sizes, we assume that 'fspec'
		 * is the only valid argument and all other fields are unused.
		 */
		struct fs_args
		{
			char *fspec;
			char  pad[150];

			fs_args() { Genode::memset(pad, 0, sizeof(pad)); }
		};

		static bool _check_type(char const *type)
		{
			for (int i = 0; fs_types[i]; i++)
				if (!Genode::strcmp(type, fs_types[i]))
					return true;
			return false;
		}

		void _print_types()
		{
			PERR("fs types:");
			for (int i = 0; fs_types[i]; i++)
				PERR("\t%s", fs_types[i]);
		}

		static char *_buffer()
		{
			/* buffer for directory entries */
			static char buf[BUFFER_SIZE];
			return buf;
		}

		Dirent_result _dirent(char const *path,
		                      struct ::dirent *dent, Dirent &vfs_dir)
		{
			/*
			 * We cannot use 'd_type' member of 'dirent' here since the EXT2
			 * implementation sets the type to unkown. Hence we use stat.
			 */
			struct stat s;
			rump_sys_lstat(path, &s);

			if (S_ISREG(s.st_mode))
				vfs_dir.type = Dirent_type::DIRENT_TYPE_FILE;
			else if (S_ISDIR(s.st_mode))
				vfs_dir.type = Dirent_type::DIRENT_TYPE_DIRECTORY;
			else if (S_ISLNK(s.st_mode))
				vfs_dir.type = Dirent_type::DIRENT_TYPE_SYMLINK;
			else if (S_ISBLK(s.st_mode))
				vfs_dir.type = Dirent_type::DIRENT_TYPE_BLOCKDEV;
			else if (S_ISCHR(s.st_mode))
				vfs_dir.type = Dirent_type::DIRENT_TYPE_CHARDEV;
			else if (S_ISFIFO(s.st_mode))
				vfs_dir.type = Dirent_type::DIRENT_TYPE_FIFO;
			else
				vfs_dir.type = Dirent_type::DIRENT_TYPE_FILE;

			strncpy(vfs_dir.name, dent->d_name, sizeof(Dirent::name));

			return DIRENT_OK;
		}

	public:

		Rump_file_system(Lock &lock, Xml_node config)
		: _lock(lock)
		{
			char fs_type[10];

			if (!config.attribute("fs").value(fs_type, 10) || !_check_type(fs_type)) {
				PERR("Invalid or no file system given (use \'<rump fs=\"<fs type>\"/>)");
				_print_types();
				throw Genode::Exception();
			}

			/* mount into extra-terrestrial-file system */
			struct fs_args args;
			int opts = config.attribute_value("writeable", true) ?
				0 : RUMP_MNT_RDONLY;

			args.fspec =  (char *)GENODE_DEVICE;
			if (rump_sys_mount(fs_type, "/", opts, &args, sizeof(args)) == -1) {
				PERR("Mounting '%s' file system failed (errno %u)", fs_type, errno);
				throw Genode::Exception();
			}

			PLOG("%s file system mounted", fs_type);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		void sync(char const *path) override { rump_sys_sync(); }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Genode::Dataspace_capability dataspace(char const *path) override
		{
			Genode::Lock::Guard guard(_lock);

			int fd = rump_sys_open(path, O_RDONLY);
			if (fd == -1) return Genode::Dataspace_capability();

			struct stat s;
			if (rump_sys_lstat(path, &s) != 0) return Genode::Dataspace_capability();

			char *local_addr = nullptr;
			Ram_dataspace_capability ds_cap;
			try {
				ds_cap = env()->ram_session()->alloc(s.st_size);

				local_addr = env()->rm_session()->attach(ds_cap);

				ssize_t n = rump_sys_read(fd, local_addr, s.st_size);
				env()->rm_session()->detach(local_addr);
				if (n != s.st_size)
					env()->ram_session()->free(ds_cap);
			} catch(...) {
				if (local_addr)
					env()->rm_session()->detach(local_addr);
				env()->ram_session()->free(ds_cap);
			}
			rump_sys_close(fd);
			return ds_cap;
		}

		void release(char const *path,
		             Genode::Dataspace_capability ds_cap) override
		{
			env()->ram_session()->free(static_cap_cast<Genode::Ram_dataspace>(ds_cap));
		}

		file_size num_dirent(char const *path) override
		{
			Genode::Lock::Guard guard(_lock);

			int fd = rump_sys_open(path, O_RDONLY | O_DIRECTORY);
			if (fd == -1)
				return 0;

			rump_sys_lseek(fd, 0, SEEK_SET);

			int       bytes = 0;
			file_size count = 0;
			char *buf = _buffer();
			do {
				bytes = rump_sys_getdents(fd, buf, BUFFER_SIZE);
				void *current, *end;
				for (current = buf, end = &buf[bytes]; current < end; current = _DIRENT_NEXT((::dirent *)current)) {
					struct ::dirent *dent = (::dirent *)current;
					if (strcmp(".", dent->d_name) && strcmp("..", dent->d_name))
						++count;
				}
			} while(bytes);

			rump_sys_close(fd);
			return count;
		}

		bool is_directory(char const *path) override
		{
			Genode::Lock::Guard guard(_lock);

			struct stat s;
			if (rump_sys_lstat(path, &s) != 0) return false;
			return S_ISDIR(s.st_mode);
		}

		char const *leaf_path(char const *path) override
		{
			Genode::Lock::Guard guard(_lock);

			struct stat s;
			return (rump_sys_lstat(path, &s) == 0) ? path : 0;
		}

		Mkdir_result mkdir(char const *path, unsigned mode) override
		{
			Genode::Lock::Guard guard(_lock);

			if (rump_sys_mkdir(path, mode|0777) != 0) switch (::errno) {
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

			/* OPEN_MODE_CREATE (or O_EXC) will not work */
			if (mode & OPEN_MODE_CREATE)
				mode |= O_CREAT;

			int fd = rump_sys_open(path, mode);
			if (fd == -1) switch (errno) {
			case ENAMETOOLONG: return OPEN_ERR_NAME_TOO_LONG;
			case EACCES:       return OPEN_ERR_NO_PERM;
			case ENOENT:       return OPEN_ERR_UNACCESSIBLE;
			case EEXIST:       return OPEN_ERR_EXISTS;
			case ENOSPC:       return OPEN_ERR_NO_SPACE;
			default:
				return OPEN_ERR_NO_PERM;
			}

			*handle = new (env()->heap()) Rump_vfs_handle(*this, mode, fd);
			return OPEN_OK;
		}

		Stat_result stat(char const *path, Stat &stat)
		{
			Genode::Lock::Guard guard(_lock);

			struct stat sb;
			if (rump_sys_lstat(path, &sb) != 0) return STAT_ERR_NO_ENTRY;

			stat.size   = sb.st_size;
			stat.mode   = sb.st_mode;
			stat.uid    = sb.st_uid;
			stat.gid    = sb.st_gid;
			stat.inode  = sb.st_ino;
			stat.device = sb.st_dev;

			return STAT_OK;
		}

		Dirent_result dirent(char const *path, file_offset index,
	                     Dirent &vfs_dir) override
		{
			Genode::Lock::Guard guard(_lock);

			int fd = rump_sys_open(path, O_RDONLY | O_DIRECTORY);
			if (fd == -1)
				return DIRENT_ERR_INVALID_PATH;

			rump_sys_lseek(fd, 0, SEEK_SET);

			int bytes;
			vfs_dir.fileno = 0;
			char *buf      = _buffer();
			struct ::dirent *dent = nullptr;
			do {
				bytes = rump_sys_getdents(fd, buf, BUFFER_SIZE);
				void *current, *end;
				for (current = buf, end = &buf[bytes]; current < end; current = _DIRENT_NEXT((::dirent *)current)) {
					dent = (::dirent *)current;
					if (strcmp(".", dent->d_name) && strcmp("..", dent->d_name)) {
						if (vfs_dir.fileno == index) {
							Path newpath(dent->d_name, path);
							rump_sys_close(fd);
							++vfs_dir.fileno;
							return _dirent(newpath.base(), dent, vfs_dir);
						}
						++vfs_dir.fileno;
					}
				}
			} while(bytes && !dent);
			rump_sys_close(fd);

			vfs_dir.type = DIRENT_TYPE_END;
			vfs_dir.name[0] = '\0';
			return DIRENT_OK;
		}

		Unlink_result unlink(char const *path) override
		{
			Genode::Lock::Guard guard(_lock);

			struct stat s;
			if (rump_sys_lstat(path, &s) == -1)
				return UNLINK_ERR_NO_ENTRY;

			if (S_ISDIR(s.st_mode)) {
				if (rump_sys_rmdir(path)  == 0) return UNLINK_OK;
			} else {
				if (rump_sys_unlink(path) == 0) return UNLINK_OK;
			}

			return UNLINK_ERR_NO_PERM;
		}

		Readlink_result readlink(char const *path, char *buf,
		                         file_size buf_size, file_size &out_len) override
		{
			Genode::Lock::Guard guard(_lock);

			ssize_t n = rump_sys_readlink(path, buf, buf_size);
			if (n == -1) {
				out_len = 0;
				return READLINK_ERR_NO_ENTRY;
			}

			out_len = n;
			return READLINK_OK;
		}

		Symlink_result symlink(char const *from, char const *to) override
		{
			Genode::Lock::Guard guard(_lock);

			if (rump_sys_symlink(from, to) != 0) switch (errno) {
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

			if (rump_sys_rename(from, to) != 0) switch (errno) {
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
			Genode::Lock::Guard guard(_lock);

			Rump_vfs_handle *handle =
				static_cast<Rump_vfs_handle *>(vfs_handle);

			ssize_t n = rump_sys_pwrite(handle->fd(), buf, buf_size, handle->seek());
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
			Genode::Lock::Guard guard(_lock);

			Rump_vfs_handle *handle =
				static_cast<Rump_vfs_handle *>(vfs_handle);

			ssize_t n = rump_sys_pread(handle->fd(), buf, buf_size, handle->seek());
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
			Genode::Lock::Guard guard(_lock);

			Rump_vfs_handle *handle =
				static_cast<Rump_vfs_handle *>(vfs_handle);

			if (rump_sys_ftruncate(handle->fd(), len) != 0) switch (errno) {
			case EACCES: return FTRUNCATE_ERR_NO_PERM;
			case EINTR:  return FTRUNCATE_ERR_INTERRUPT;
			case ENOSPC: return FTRUNCATE_ERR_NO_SPACE;
			default:
				return FTRUNCATE_ERR_NO_PERM;
			}
			return FTRUNCATE_OK;
		}
};

