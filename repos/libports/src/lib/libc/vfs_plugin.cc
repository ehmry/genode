/*
 * \brief   Libc plugin for using a process-local virtual file system
 * \author  Norman Feske
 * \date    2014-04-09
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>

/* libc includes */
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <dlfcn.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <vfs_plugin.h>

/* libc-internal includes */
#include <libc_mem_alloc.h>
#include "libc_errno.h"
#include "timeout.h"

/**
 * Utility to convert VFS stat struct to the libc stat struct
 *
 * Code shared between 'stat' and 'fstat'.
 */
static void vfs_stat_to_libc_stat_struct(Vfs::Directory_service::Stat const &src,
                                         struct stat *dst)
{
	enum { FS_BLOCK_SIZE = 1024 };

	Genode::memset(dst, 0, sizeof(*dst));

	dst->st_uid     = src.uid;
	dst->st_gid     = src.gid;
	dst->st_mode    = src.mode;
	dst->st_size    = src.size;
	dst->st_blksize = FS_BLOCK_SIZE;
	dst->st_blocks  = (dst->st_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
	dst->st_ino     = src.inode;
	dst->st_dev     = src.device;
}


namespace Libc {

	Genode::Xml_node config() __attribute__((weak));
	Genode::Xml_node config()
	{
		return Genode::config()->xml_node().sub_node("libc");
	}


	Genode::Xml_node vfs_config() __attribute__((weak));
	Genode::Xml_node vfs_config()
	{
		return Libc::config().sub_node("vfs");
	}


	class Config_attr
	{
		private:

			char _buf[Vfs::MAX_PATH_LEN];

		public:

			Config_attr(char const *attr_name, char const *default_value)
			{
				Genode::strncpy(_buf, default_value, sizeof(_buf));
				try {
					Libc::config().attribute(attr_name).value(_buf, sizeof(_buf));
				} catch (...) { }
			}

			char const *string() const { return _buf; }
	};


	char const *initial_cwd() __attribute__((weak));
	char const *initial_cwd()
	{
		static Config_attr initial_cwd("cwd", "/");
		return initial_cwd.string();
	}


	char const *config_stdin() __attribute__((weak));
	char const *config_stdin()
	{
		static Config_attr stdin("stdin", "");
		return stdin.string();
	}


	char const *config_stdout() __attribute__((weak));
	char const *config_stdout()
	{
		static Config_attr stdout("stdout", "");
		return stdout.string();
	}


	char const *config_stderr() __attribute__((weak));
	char const *config_stderr()
	{
		static Config_attr stderr("stderr", "");
		return stderr.string();
	}

	char const *config_rtc() __attribute__((weak));
	char const *config_rtc()
	{
		static Config_attr rtc("rtc", "");
		return rtc.string();
	}

	char const *config_socket() __attribute__((weak));
	char const *config_socket()
	{
		static Config_attr socket("socket", "");
		return socket.string();
	}

	char const *config_pipe() __attribute__((weak));
	char const *config_pipe()
	{
		static Config_attr pipe("pipe", "");
		return pipe.string();
	}
}


int Libc::Vfs_plugin::access(const char *path, int amode)
{
	if (_root_dir.leaf_path(path))
		return 0;

	errno = ENOENT;
	return -1;
}


Libc::File_descriptor *Libc::Vfs_plugin::open(char const *path, int flags,
                                              int libc_fd)
{
	Genode::Lock::Guard guard(_lock);

	Context *context = _open(path, flags);
	if (context == nullptr)
		return nullptr;

	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->alloc(this, context, libc_fd);

	/*
	 * set normal files non-blocking so that
	 * zero reads simply return zero
	 */
	fd->status = flags | O_NONBLOCK;

	if ((flags & O_TRUNC) && (ftruncate(fd, 0) == -1)) {
		close(fd);
		Errno(EINVAL);
		return nullptr;
	}

	return fd;
}


int Libc::Vfs_plugin::close(Libc::File_descriptor *fd)
{
	Genode::Lock::Guard guard(_lock);

	Libc::Vfs_plugin::Context *context = vfs_context(fd);

	destroy(_alloc, context);
	Libc::file_descriptor_allocator()->free(fd);

	return 0;
}


int Libc::Vfs_plugin::dup2(Libc::File_descriptor *fd,
                           Libc::File_descriptor *new_fd)
{
	new_fd->context = fd->context;
	return new_fd->libc_fd;
}


int Libc::Vfs_plugin::fstat(Libc::File_descriptor *fd, struct stat *buf)
{
	return stat(fd->fd_path, buf);
}


int Libc::Vfs_plugin::fstatfs(Libc::File_descriptor *fd, struct statfs *buf)
{
	if (!fd || !buf)
		return Libc::Errno(EFAULT);

	Genode::memset(buf, 0, sizeof(*buf));

	buf->f_flags = MNT_UNION;
	return 0;
}


int Libc::Vfs_plugin::mkdir(const char *path, mode_t mode)
{
	Genode::Lock::Guard guard(_lock);

	typedef Vfs::Directory_service::Mkdir_result Result;

	switch (_root_dir.mkdir(path, mode)) {
	case Result::MKDIR_ERR_EXISTS:        errno = EEXIST;       return -1;
	case Result::MKDIR_ERR_NO_ENTRY:      errno = ENOENT;       return -1;
	case Result::MKDIR_ERR_NO_SPACE:      errno = ENOSPC;       return -1;
	case Result::MKDIR_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; return -1;
	case Result::MKDIR_ERR_NO_PERM:       errno = EPERM;        return -1;
	case Result::MKDIR_OK:                                      break;
	}
	return 0;
}


int Libc::Vfs_plugin::stat(char const *path, struct stat *buf)
{
	Genode::Lock::Guard guard(_lock);

	if (!path or !buf) {
		errno = EFAULT;
		return -1;
	}

	typedef Vfs::Directory_service::Stat_result Result;

	Vfs::Directory_service::Stat stat;

	switch (_root_dir.stat(path, stat)) {
	case Result::STAT_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::STAT_ERR_NO_PERM:  errno = EACCES; return -1;
	case Result::STAT_OK:                           break;
	}

	vfs_stat_to_libc_stat_struct(stat, buf);
	return 0;
}


Libc::Vfs_plugin::Context *Libc::Vfs_plugin::_open(const char *path, unsigned flags)
{
	typedef Vfs::Directory_service::Open_result Result;

	Vfs::Vfs_handle *handle = nullptr;

	do {
		switch (_root_dir.open(path, flags, &handle, _alloc)) {

		case Result::OPEN_OK:
			break;

		case Result::OPEN_ERR_UNACCESSIBLE:
			{
				if (!(flags & O_CREAT)) {
					Errno(ENOENT);
					return nullptr;
				}

				/* O_CREAT is set, so try to create the file */
				switch (_root_dir.open(path, flags | O_EXCL, &handle, _alloc)) {

				case Result::OPEN_OK:
					break;

				case Result::OPEN_ERR_NO_PERM:       Errno(EPERM);        return nullptr;
				case Result::OPEN_ERR_EXISTS:
					if ((flags & O_CREAT) && (!(flags&O_EXCL))) {
						flags ^= O_CREAT;
						break;
					} else {
						Errno(EEXIST);
						return nullptr;
					}
				case Result::OPEN_ERR_UNACCESSIBLE:  Errno(ENOENT);       return nullptr;
				case Result::OPEN_ERR_NAME_TOO_LONG: Errno(ENAMETOOLONG); return nullptr;
				case Result::OPEN_ERR_NO_SPACE:      Errno(ENOSPC);       return nullptr;
				}
			}
			break;

		case Result::OPEN_ERR_NO_PERM:       Errno(EPERM);        return nullptr;
		case Result::OPEN_ERR_EXISTS:        Errno(EEXIST);       return nullptr;
		case Result::OPEN_ERR_NAME_TOO_LONG: Errno(ENAMETOOLONG); return nullptr;
		case Result::OPEN_ERR_NO_SPACE:      Errno(ENOSPC);       return nullptr;
		}
	} while (handle == nullptr);

	/* the file was successfully opened */
	try { return new (_alloc) Context(*handle); }
	catch (Genode::Allocator::Out_of_memory) { Errno(ENOMEM); return nullptr; }
}


namespace Vfs { struct Libc_write_callback; }

/**
 * VFS write callback with lifetime management
 */
struct Vfs::Libc_write_callback : Write_callback
{
	Libc::Task &task;
	Vfs_handle &handle;

	char const * const buffer;
	file_size const buffer_len;
	file_size accumulator = 0;
	Callback::Status status = PARTIAL;

	Libc_write_callback(Libc::Task &t, Vfs_handle &h, char const *buf, file_size len)
	: task(t), handle(h), buffer(buf), buffer_len(len) {
		handle.write_callback(*this); }

	~Libc_write_callback() {
		handle.drop_write(); }

	file_size write(char *dst, file_size len, Callback::Status st) override
	{
		status = st;

		file_size out = min(len, buffer_len-accumulator);
		if (out) {
			if (dst) {
				char const *p = buffer+accumulator;
				Genode::memcpy(dst, p, out);
			}
			accumulator += out;
		}

		if (status != PARTIAL)
			task.unblock();

		return out;
	}
};

ssize_t Libc::Vfs_plugin::_write(Vfs::Vfs_handle &handle, const void *buf,
                                 Vfs::file_size count)
{
	using namespace Vfs;

	Task &task = Libc::this_task();
	Libc_write_callback cb(task, handle, (char const *)buf, count);

	handle.fs().write(&handle, count);
	while (cb.status == Callback::PARTIAL)
		_yield_vfs(task);

	switch (cb.status) {
	case Callback::COMPLETE:       break;
	case Callback::ERR_INVALID:    return Errno(EINVAL);
	case Callback::ERR_IO:         return Errno(EIO);
	case Callback::ERR_TERMINATED: break; /* not defined as an error in POSIX */
	case Callback::PARTIAL:        break;
	}

	handle.advance_seek(cb.accumulator);
	return cb.accumulator;
}


ssize_t Libc::Vfs_plugin::write(Libc::File_descriptor *fd, const void *buf,
                                ::size_t count)
{
	Genode::Lock::Guard guard(_lock);

	Libc::Vfs_plugin::Context *context = vfs_context(fd);
	return context ? _write(context->handle(), buf, count) : Errno(EBADF);
}


namespace Vfs { struct Libc_read_callback; }

/**
 * VFS write callback with lifetime management
 */
struct Vfs::Libc_read_callback : Read_callback
{
	Libc::Task &task;
	Vfs_handle &handle;

	char * const buffer;
	file_size const buffer_len;
	file_size accumulator = 0;
	Callback::Status status = PARTIAL;

	Libc_read_callback(Libc::Task &t, Vfs_handle &h, char *buf, file_size len)
	: task(t), handle(h), buffer(buf), buffer_len(len) {
		handle.read_callback(*this); }

	~Libc_read_callback() {
		handle.drop_read(); }

	file_size read(char const *src, file_size len, Callback::Status st) override
	{
		status = st;
		file_size out = min(len, buffer_len-accumulator);

		if (out) {
			char *p = buffer+accumulator;
			if (src)
				Genode::memcpy(p, src, out);
			else
				Genode::memset(p, 0x00, out);
			accumulator += out;
		}
		if (status != PARTIAL)
			task.unblock();

		return out;
	}
};

ssize_t Libc::Vfs_plugin::_read(Context &context, void *buf,
                                Vfs::file_size count, bool blocking)
{
	using namespace Vfs;

	Task &task = Libc::this_task();
	Vfs::Vfs_handle &handle = context.handle();
	Libc_read_callback cb(task, handle, (char*)buf, count);

	handle.fs().read(&handle, count);

	while (cb.status == Callback::PARTIAL)
		_yield_vfs(task);

	while (blocking && (cb.status == Callback::COMPLETE) && (cb.accumulator == 0)) {
		if (!context.subscribed() && !context.subscribe()) {
			Genode::error("refusing blocking read, notifications not supported");
			return Errno(EIO);
		}
		context.reset();

		Task_resume_callback notify_cb(task);
		notify_cb.add_context(context);

		timeval const &timeout = context.timeout();
		if (timeout.tv_sec || timeout.tv_usec) {
			Libc::Timeout task_timeout(task, timeout.tv_sec*1000+timeout.tv_usec/1000);
			_yield_vfs(task);
			if (task_timeout.triggered()) {
				return Errno(EWOULDBLOCK);
			}
		} else {
			_yield_vfs(task);
		}
		handle.fs().read(&handle, count);
		while (cb.status == Callback::PARTIAL)
			_yield_vfs(task);
	}

	switch (cb.status) {
	case Callback::COMPLETE:       break;
	case Callback::ERR_INVALID:    return Errno(EINVAL);
	case Callback::ERR_IO:         return Errno(EIO);
	case Callback::ERR_TERMINATED: break; /* not defined as an error in POSIX */
	case Callback::PARTIAL:        break; /* should never be reached */
	}

	handle.advance_seek(cb.accumulator);
	return cb.accumulator;
}


ssize_t Libc::Vfs_plugin::read(Libc::File_descriptor *fd, void *buf,
                               ::size_t count)
{
	Genode::Lock::Guard guard(_lock);

	Libc::Vfs_plugin::Context *context = vfs_context(fd);
	if (!context)
		return Errno(EBADF);

	return _read(*context, buf, count, !(fd->status&O_NONBLOCK));
}


ssize_t Libc::Vfs_plugin::getdirentries(Libc::File_descriptor *fd, char *buf,
                                        ::size_t nbytes, ::off_t *basep)
{
	Genode::Lock::Guard guard(_lock);

	if (nbytes < sizeof(struct dirent)) {
		Genode::error("getdirentries: buffer too small");
		return -1;
	}

	typedef Vfs::Directory_service::Dirent_result Result;

	Context *context = vfs_context(fd);
	Vfs::Vfs_handle *handle = &context->handle();

	Vfs::Directory_service::Dirent dirent_out;
	Genode::memset(&dirent_out, 0, sizeof(dirent_out));

	unsigned const index = handle->seek() / sizeof(Vfs::Directory_service::Dirent);

	switch (handle->ds().dirent(fd->fd_path, index, dirent_out)) {
	case Result::DIRENT_ERR_INVALID_PATH: errno = ENOENT; return -1;
	case Result::DIRENT_ERR_NO_PERM:      errno = EACCES; return -1;
	case Result::DIRENT_OK:                               break;
	}

	/*
	 * Convert dirent structure from VFS to libc
	 */

	struct dirent *dirent = (struct dirent *)buf;
	Genode::memset(dirent, 0, sizeof(struct dirent));

	switch (dirent_out.type) {
	case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY: dirent->d_type = DT_DIR;  break;
	case Vfs::Directory_service::DIRENT_TYPE_FILE:      dirent->d_type = DT_REG;  break;
	case Vfs::Directory_service::DIRENT_TYPE_SYMLINK:   dirent->d_type = DT_LNK;  break;
	case Vfs::Directory_service::DIRENT_TYPE_FIFO:      dirent->d_type = DT_FIFO; break;
	case Vfs::Directory_service::DIRENT_TYPE_CHARDEV:   dirent->d_type = DT_CHR;  break;
	case Vfs::Directory_service::DIRENT_TYPE_BLOCKDEV:  dirent->d_type = DT_BLK;  break;
	case Vfs::Directory_service::DIRENT_TYPE_END:                                 return 0;
	}

	dirent->d_fileno = dirent_out.fileno;
	dirent->d_reclen = sizeof(struct dirent);

	Genode::strncpy(dirent->d_name, dirent_out.name, sizeof(dirent->d_name));

	dirent->d_namlen = Genode::strlen(dirent->d_name);

	/*
	 * Keep track of VFS seek pointer and user-supplied basep.
	 */
	handle->advance_seek(sizeof(Vfs::Directory_service::Dirent));

	*basep += sizeof(struct dirent);

	return sizeof(struct dirent);
}


int Libc::Vfs_plugin::ioctl(Libc::File_descriptor *fd, int request, char *argp)
{
	Genode::Lock::Guard guard(_lock);

	using ::off_t;

	/*
	 * Marshal ioctl arguments
	 */

	typedef Vfs::File_io_service::Ioctl_opcode Opcode;

	Opcode opcode = Opcode::IOCTL_OP_UNDEFINED;

	Vfs::File_io_service::Ioctl_arg arg = 0;

	switch (request) {

	case TIOCGWINSZ:
		opcode = Opcode::IOCTL_OP_TIOCGWINSZ;
		break;

	case TIOCGETA:
		{
			::termios *termios = (::termios *)argp;

			termios->c_iflag = 0;
			termios->c_oflag = 0;
			termios->c_cflag = 0;
			/*
			 * Set 'ECHO' flag, needed by libreadline. Otherwise, echoing
			 * user input doesn't work in bash.
			 */
			termios->c_lflag = ECHO;
			::memset(termios->c_cc, _POSIX_VDISABLE, sizeof(termios->c_cc));
			termios->c_ispeed = 0;
			termios->c_ospeed = 0;

			return 0;
		}

		break;

	case TIOCSETAF:
		{
			opcode = Opcode::IOCTL_OP_TIOCSETAF;

			::termios *termios = (::termios *)argp;

			/*
			 * For now only enabling/disabling of ECHO is supported
			 */
			if (termios->c_lflag & (ECHO | ECHONL)) {
				arg = (Vfs::File_io_service::IOCTL_VAL_ECHO |
				       Vfs::File_io_service::IOCTL_VAL_ECHONL);
			}
			else {
				arg = Vfs::File_io_service::IOCTL_VAL_NULL;
			}

			break;
		}

	case TIOCSETAW:
		{
			opcode = Opcode::IOCTL_OP_TIOCSETAW;
			arg    = argp ? *(int*)argp : 0;
			break;
		}

	case FIONBIO:
		{
			opcode = Opcode::IOCTL_OP_FIONBIO;
			arg    = argp ? *(int*)argp : 0;
			break;
		}

	case DIOCGMEDIASIZE:
		{
			opcode = Opcode::IOCTL_OP_DIOCGMEDIASIZE;
			arg    = 0;
			break;
		}

	default:
		Genode::warning("unsupported ioctl (request=", Genode::Hex(request), ")");
		break;
	}

	if (opcode == Opcode::IOCTL_OP_UNDEFINED) {
		errno = ENOTTY;
		return -1;
	}

	typedef Vfs::File_io_service::Ioctl_result Result;

	Vfs::File_io_service::Ioctl_out out;
	Genode::memset(&out, 0, sizeof(out));

	Context *context = vfs_context(fd);
	Vfs::Vfs_handle *handle = &context->handle();

	switch (handle->fs().ioctl(handle, opcode, arg, out)) {
	case Result::IOCTL_ERR_INVALID: errno = EINVAL; return -1;
	case Result::IOCTL_ERR_NOTTY:   errno = ENOTTY; return -1;
	case Result::IOCTL_OK:                          break;
	}

	/*
	 * Unmarshal ioctl results
	 */
	switch (request) {

	case TIOCGWINSZ:
		{
			::winsize *winsize = (::winsize *)arg;
			winsize->ws_row = out.tiocgwinsz.rows;
			winsize->ws_col = out.tiocgwinsz.columns;
			return 0;
		}
	case TIOCSETAF:
	case TIOCSETAW:
		return 0;

	case FIONBIO:
		return 0;

	case DIOCGMEDIASIZE:
		{
			/* resolve ambiguity with libc type */
			using Genode::int64_t;

			int64_t *disk_size = (int64_t*)arg;
			*disk_size = out.diocgmediasize.size;
			return 0;
		}

	default:
		break;
	}

	return -1;
}


::off_t Libc::Vfs_plugin::lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
{
	Context *context = vfs_context(fd);
	Vfs::Vfs_handle *handle = &context->handle();

	switch (whence) {
	case SEEK_SET: handle->seek(offset); break;
	case SEEK_CUR: handle->advance_seek(offset); break;
	case SEEK_END:
		{
			struct stat stat;
			::memset(&stat, 0, sizeof(stat));
			fstat(fd, &stat);
			handle->seek(stat.st_size + offset);
		}
		break;
	}
	return handle->seek();
}


int Libc::Vfs_plugin::ftruncate(Libc::File_descriptor *fd, ::off_t length)
{
	Context *context = vfs_context(fd);
	Vfs::Vfs_handle *handle = &context->handle();

	typedef Vfs::File_io_service::Ftruncate_result Result;

	switch (handle->fs().ftruncate(handle, length)) {
	case Result::FTRUNCATE_ERR_NO_PERM:   errno = EPERM;  return -1;
	case Result::FTRUNCATE_ERR_INTERRUPT: errno = EINTR;  return -1;
	case Result::FTRUNCATE_ERR_NO_SPACE:  errno = ENOSPC; return -1;
	case Result::FTRUNCATE_OK:                            break;
	}
	return 0;
}


int Libc::Vfs_plugin::fcntl(Libc::File_descriptor *fd, int cmd, long arg)
{
	switch (cmd) {
	case F_DUPFD:
		{
			/*
			 * Allocate free file descriptor locally.
			 */
			Libc::File_descriptor *new_fd =
				Libc::file_descriptor_allocator()->alloc(this, 0);
			new_fd->path(fd->fd_path);

			/*
			 * Use new allocated number as name of file descriptor
			 * duplicate.
			 */
			if (dup2(fd, new_fd) == -1) {
				Genode::error("Plugin::fcntl: dup2 unexpectedly failed");
				errno = EINVAL;
				return -1;
			}

			return new_fd->libc_fd;
		}

	case F_GETFD:
	case F_SETFD: return 0;

	case F_GETFL: return fd->status;
	case F_SETFL: fd->status = arg; return 0;

	default: break;
	}

	Genode::error("fcntl(): command ", cmd, " not supported - vfs");
	return Errno(EINVAL);
}


int Libc::Vfs_plugin::fsync(Libc::File_descriptor *fd)
{
	Genode::Lock::Guard guard(_lock);

	_root_dir.sync(fd->fd_path);
	return 0;
}


int Libc::Vfs_plugin::symlink(const char *oldpath, const char *newpath)
{
	Genode::Lock::Guard guard(_lock);

	typedef Vfs::Directory_service::Symlink_result Result;

	switch (_root_dir.symlink(oldpath, newpath)) {
	case Result::SYMLINK_ERR_EXISTS:        errno = EEXIST;       return -1;
	case Result::SYMLINK_ERR_NO_ENTRY:      errno = ENOENT;       return -1;
	case Result::SYMLINK_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; return -1;
	case Result::SYMLINK_ERR_NO_PERM:       errno = ENOSYS;       return -1;
	case Result::SYMLINK_ERR_NO_SPACE:      errno = ENOSPC;       return -1;
	case Result::SYMLINK_OK:                                      break;
	}
	return 0;
}


ssize_t Libc::Vfs_plugin::readlink(const char *path, char *buf, ::size_t buf_size)
{
	Genode::Lock::Guard guard(_lock);

	typedef Vfs::Directory_service::Readlink_result Result;

	Vfs::file_size out_len = 0;

	switch (_root_dir.readlink(path, buf, buf_size, out_len)) {
	case Result::READLINK_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::READLINK_ERR_NO_PERM:  errno = EACCES; return -1;
	case Result::READLINK_OK:                           break;
	};

	return out_len;
}


int Libc::Vfs_plugin::rmdir(char const *path)
{
	return unlink(path);
}


int Libc::Vfs_plugin::unlink(char const *path)
{
	Genode::Lock::Guard guard(_lock);

	typedef Vfs::Directory_service::Unlink_result Result;

	switch (_root_dir.unlink(path)) {
	case Result::UNLINK_ERR_NO_ENTRY:  errno = ENOENT;    return -1;
	case Result::UNLINK_ERR_NO_PERM:   errno = EPERM;     return -1;
	case Result::UNLINK_ERR_NOT_EMPTY: errno = ENOTEMPTY; return -1;
	case Result::UNLINK_OK:            break;
	}
	return 0;
}


int Libc::Vfs_plugin::rename(char const *from_path, char const *to_path)
{
	Genode::Lock::Guard guard(_lock);

	typedef Vfs::Directory_service::Rename_result Result;

	if (_root_dir.leaf_path(to_path)) {

		if (_root_dir.directory(to_path)) {
			if (!_root_dir.directory(from_path)) {
				errno = EISDIR; return -1;
			}

			if (_root_dir.num_dirent(to_path)) {
				errno = ENOTEMPTY; return -1;
			}

		} else {
			if (_root_dir.directory(from_path)) {
				errno = ENOTDIR; return -1;
			}
		}
	}

	switch (_root_dir.rename(from_path, to_path)) {
	case Result::RENAME_ERR_NO_ENTRY: errno = ENOENT; return -1;
	case Result::RENAME_ERR_CROSS_FS: errno = EXDEV;  return -1;
	case Result::RENAME_ERR_NO_PERM:  errno = EPERM;  return -1;
	case Result::RENAME_OK:                           break;
	}
	return 0;
}


void *Libc::Vfs_plugin::mmap(void *addr_in, ::size_t length, int prot, int flags,
                             Libc::File_descriptor *fd, ::off_t offset)
{
	if (prot != PROT_READ) {
		Genode::error("mmap for prot=", Genode::Hex(prot), " not supported");
		errno = EACCES;
		return (void *)-1;
	}

	if (addr_in != 0) {
		Genode::error("mmap for predefined address not supported");
		errno = EINVAL;
		return (void *)-1;
	}

	/*
	 * XXX attempt to obtain memory mapping via
	 *     'Vfs::Directory_service::dataspace'.
	 */

	void *addr = Libc::mem_alloc()->alloc(length, PAGE_SHIFT);
	if (addr == (void *)-1) {
		errno = ENOMEM;
		return (void *)-1;
	}

	if (::pread(fd->libc_fd, addr, length, offset) < 0) {
		Genode::error("mmap could not obtain file content");
		::munmap(addr, length);
		errno = EACCES;
		return (void *)-1;
	}

	return addr;
}


int Libc::Vfs_plugin::munmap(void *addr, ::size_t)
{
	Libc::mem_alloc()->free(addr);
	return 0;
}

int Libc::Vfs_plugin::_select(int nfds,
                             fd_set *read_out,  fd_set const &read_in,
                             fd_set *write_out, fd_set const &write_in)
{
	int nready = 0;
	for (int libc_fd = 0; libc_fd < nfds; ++libc_fd) {
		Libc::File_descriptor *fdo =
			Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);

		/* handle only libc_fds that belong to this plugin */
		if (!fdo || (fdo->plugin != this))
			continue;
		/* XXX: eventually they all come from this plugin */

		Context *context = vfs_context(fdo);

		if (read_out && FD_ISSET(libc_fd, &read_in) && context->notifications()) {
			//context->reset();
			FD_SET(libc_fd, read_out);
			++nready;
		}

		if (write_out && FD_ISSET(libc_fd, &write_in) && context->notifications()) {
			//context->reset();
			FD_SET(libc_fd, write_out);
			++nready;
		}
	}

	return nready;
}


int Libc::Vfs_plugin::select(int nfds, fd_set *readfds, fd_set *writefds,
                             fd_set *exceptfds, struct timeval *timeout)
{
	fd_set read_in;
	fd_set write_in;
	FD_ZERO(&read_in);
	FD_ZERO(&write_in);

	if (readfds) {
		read_in = *readfds;
		FD_ZERO(readfds);
	}

	if (writefds) {
		write_in = *writefds;
		FD_ZERO(writefds);
	}

	if (exceptfds)
		FD_ZERO(exceptfds);

	/* this resumes the task when a context is notified */
	Task &task = Libc::this_task();
	Task_resume_callback resume_cb(task);

	Genode::Lock::Guard guard(_lock);

	/* set callbacks on descriptors and remove those that do not notify */
	for (int libc_fd = 0; libc_fd < nfds; ++libc_fd) {
		Libc::File_descriptor *fdo =
			Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);

		/* handle only libc_fds that belong to this plugin */
		if (!fdo || (fdo->plugin != this))
			continue;
		/* XXX: eventually they all come from this plugin */

		Context *ctx = vfs_context(fdo);
		if ((FD_ISSET(libc_fd, &read_in)) || (FD_ISSET(libc_fd, &write_in))) {
			if (!ctx->subscribed()) {
				if (!ctx->subscribe()) {
					/* drop it, no notificatigions */
					FD_CLR(libc_fd, &read_in);
					FD_CLR(libc_fd, &write_in);
					continue;
				}
			}
			resume_cb.add_context(*ctx);
		}
	}

	int nready = _select(nfds, readfds, read_in, writefds, write_in);
	if (nready == 0) {
		if (timeout == NULL) {
			do {
				_yield_vfs(task);
				nready = _select(nfds, readfds, read_in, writefds, write_in);
			} while (nready == 0);
		} else {
			Libc::Timeout task_timeout(task, timeout->tv_sec*1000+timeout->tv_usec/1000);
			do {
				_yield_vfs(task);
				nready = _select(nfds, readfds, read_in, writefds, write_in);
			} while (!nready && !task_timeout.triggered());
		}
	}

	return nready;
}


int Libc::Vfs_plugin::pipe(Libc::File_descriptor *fildes[2])
{
	Genode::Lock::Guard guard(_lock);

	using namespace Vfs;

	Absolute_path const meta_path("new_pipe", Libc::config_pipe());

	Context *meta_ctx = _open(meta_path.base(), Directory_service::OPEN_MODE_RDONLY);
	if (meta_ctx == nullptr) {
		Genode::error("failed to open '", meta_path, "', pipe VFS plugin not loaded?");
		return -1;
	}
	Context::Guard meta_guard(_alloc, meta_ctx);

	char pipe_name[16];
	::ssize_t n =  _read(*meta_ctx, pipe_name, sizeof(pipe_name), false);
	if (n == -1)
		return -1; /* read has set errno */

	if (pipe_name[n-1] == '\n')
		--n;
	pipe_name[n] = '\0';
	Absolute_path const pipe_path(pipe_name, Libc::config_pipe());

	Vfs::Vfs_handle *pipe_in  = nullptr;
	Vfs::Vfs_handle *pipe_out = nullptr;

	switch (_root_dir.open(pipe_path.base(), Directory_service::OPEN_MODE_RDONLY, &pipe_in, _alloc)) {
	case Directory_service::OPEN_OK:           break;
	case Directory_service::OPEN_ERR_NO_SPACE: return Errno(ENOMEM);
	default: return Errno(EINVAL);
	}

	switch (_root_dir.open(pipe_path.base(), Directory_service::OPEN_MODE_WRONLY, &pipe_out, _alloc)) {
	case Directory_service::OPEN_OK: break;
	case Directory_service::OPEN_ERR_NO_SPACE:
		pipe_in->ds().close(pipe_in);
		return Errno(ENOMEM);
	default:
		pipe_in->ds().close(pipe_in);
		return Errno(EINVAL);
	}

	Context *context_in, *context_out;
	try { context_in = new (_alloc) Context(*pipe_in); }
	catch (Genode::Allocator::Out_of_memory) { return Errno(ENOMEM); }
	try { context_out = new (_alloc) Context(*pipe_out); }
	catch (Genode::Allocator::Out_of_memory) {
		destroy(_alloc, context_in);
		return Errno(ENOMEM);
	}

	fildes[0] = Libc::file_descriptor_allocator()->alloc(this, vfs_context(context_in));
	fildes[1] = Libc::file_descriptor_allocator()->alloc(this, vfs_context(context_out));

	return 0;
}