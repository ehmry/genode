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

#ifndef _LIBC_VFS__PLUGIN_H_
#define _LIBC_VFS__PLUGIN_H_

/* Genode includes */
#include <base/env.h>
#include <vfs/dir_file_system.h>

/* libc includes */
#include <fcntl.h>
#include <unistd.h>

/* libc plugin interface */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

#include "task.h"

#include <base/debug.h>

namespace Libc {

	Genode::Xml_node config();
	Genode::Xml_node vfs_config();

	char const *initial_cwd();
	char const *config_stdin();
	char const *config_stdout();
	char const *config_stderr();

}

struct Pdbg_guard
{
	char const *func;

	Pdbg_guard(char const *name) : func(name) {
		Genode::log("enter function ",func); }

	~Pdbg_guard() {
		Genode::log("exit function ",func); }
};

namespace Libc { class Vfs_plugin; }


class Libc::Vfs_plugin : public Libc::Plugin
{
	public:

		class Context :
			public Libc::Plugin_context,
			public Vfs::Notify_callback,
			public Genode::List<Context>::Element
		{
			protected:

				Vfs::Vfs_handle *_handle;

			private:

				struct timeval _timeout { 0, 0 };

				Notify_callback *_inner_callback = nullptr;

				int  _notifications = 0;
				bool _subscribed = false;

			public:

				Context(Vfs::Vfs_handle &h) : _handle(&h) { }

				~Context() { _handle->ds().close(_handle); }

				Vfs::Vfs_handle &handle() { return *_handle; }

				void handle(Vfs::Vfs_handle &h)
				{
					_handle = &h;
					_inner_callback = nullptr;
					_notifications = 0;
					_subscribed = false;
				}

				void notify_callback(Vfs::Notify_callback &inner) {
					_inner_callback = &inner; }

				void drop_notify() {
					_inner_callback = nullptr; }

				bool subscribe()
				{
					if (!_subscribed) {
						_subscribed = _handle->ds().subscribe(_handle);
						if (_subscribed)
							_handle->notify_callback(*this);
					}
					return _subscribed;
				}

				void unsubscribe()
				{
					_subscribed = false;
					_handle->drop_notify();
				}

				bool   subscribed() const { return _subscribed; }
				int notifications() const { return _notifications; }

				void ack() { if (_notifications > 0) --_notifications; }

				void reset() { _notifications = 0; }

				void timeout(timeval &to) { _timeout = to; }
				timeval const &timeout() { return _timeout; }

				/*******************************
				 ** Notify callback interface **
				 *******************************/

				void notify() override
				{
					++_notifications;
					if (_inner_callback)
						_inner_callback->notify();
				}

				struct Guard
				{
					Genode::Allocator &alloc;
					Context           &context;

					Guard(Genode::Allocator &a, Context *c)
					: alloc(a), context(*c) { }

					~Guard() { destroy(alloc, &context); }
				};
		};

		struct Task_resume_callback : Vfs::Notify_callback
		{
			Task &task;
			Genode::List<Context> contexts;

			Task_resume_callback(Task &t) : task(t) { }

			~Task_resume_callback()
			{
				for (Context *c = contexts.first(); c; c = contexts.first()) {
					c->drop_notify();
					contexts.remove(c);
				}
			}

			void add_context(Context &c)
			{
				contexts.insert(&c);
				c.notify_callback(*this);
			};

			void notify() override {
				task.unblock(); }
		};

		struct Meta_path : Libc::Absolute_path
		{
			Meta_path() { }

			Meta_path(char const *path, char const *pwd = 0)
			: Absolute_path(path, pwd)
			{
				remove_trailing('\n');
			}
		};

		struct Socket_context final : Context
		{
			Meta_path const path;

			Vfs::Vfs_handle *_accept_context = nullptr;

			Socket_context(Absolute_path const &path, Vfs::Vfs_handle &data_handle)
			: Context(data_handle), path(path.base()) { }

			virtual ~Socket_context() { }

			void accept_handle(Vfs::Vfs_handle *accept_handle)
			{
				if (_handle != accept_handle) {
					_handle->ds().close(_handle);
					_handle = accept_handle;
				}
			}
		};

		static Libc::Vfs_plugin::Context *vfs_context(Libc::File_descriptor *fd)
		{
			return dynamic_cast<Libc::Vfs_plugin::Context *>(fd->context);
		}

		static Libc::Plugin_context *vfs_context(Libc::Vfs_plugin::Context *context)
		{
			return reinterpret_cast<Libc::Plugin_context *>(context);
		}

	private:

		Genode::Allocator &_alloc;

		Vfs::Dir_file_system _root_dir;

		Genode::Lock _lock;

		/**
		 * Unlock the VFS plugin, suspend task, lock on resume
		 */
		inline void _yield_vfs(Task &task)
		{
			_lock.unlock();
			task.block();
			_lock.lock();
		}

		Genode::Xml_node _vfs_config()
		{
			try {
				return vfs_config();
			} catch (...) {
				Genode::warning("no VFS configured");
				return Genode::Xml_node("<vfs/>");
			}
		}

		void _open_stdio(int libc_fd, char const *path, unsigned flags)
		{
			struct stat out_stat;
			if (::strlen(path) == 0 || stat(path, &out_stat) != 0)
				return;

			Libc::File_descriptor *fd = open(path, flags, libc_fd);
			if (!fd) {
				Genode::error("could not open '", path, "' for stdio");
				return;
			}

			if (fd->libc_fd != libc_fd) {
				Genode::error("could not allocate fd ",libc_fd," for ",path,
				              ", got fd ",fd->libc_fd);
				close(fd);
				return;
			}

			/* make stdio blocking */
			fd->status = fd->status & ~O_NONBLOCK;

			/*
			 * We need to manually register the path. Normally this is done
			 * by '_open'. But we call the local 'open' function directly
			 * because we want to explicitly specify the libc fd ID.
			 *
			 * We have to allocate the path from the libc (done via 'strdup')
			 * such that the path can be freed when an stdio fd is closed.
			 */
			fd->fd_path = strdup(path);
		}

		Context *_open(const char *path, unsigned flags);

		ssize_t _read(Context&, void*, Vfs::file_size, bool blocking);
		ssize_t _write(Vfs::Vfs_handle&, void const *, Vfs::file_size);

		int read_sockaddr_in(char const *file_name,
		                     Libc::File_descriptor *fd,
		                     struct sockaddr_in *addr,
		                     socklen_t *addrlen);

	public:

		/**
		 * Constructor
		 */
		Vfs_plugin(Genode::Env &env, Genode::Allocator &alloc)
		:
			_alloc(alloc),
			_root_dir(env, _alloc, _vfs_config(),
			          Vfs::global_file_system_factory())
		{
			if (_root_dir.num_dirent("/")) {
				chdir(initial_cwd());

				_open_stdio(0, config_stdin(),  O_RDONLY);
				_open_stdio(1, config_stdout(), O_WRONLY);
				_open_stdio(2, config_stderr(), O_WRONLY);
			}
		}

		~Vfs_plugin() { }

		bool supports_access(const char *, int)                  override { return true; }
		bool supports_mkdir(const char *, mode_t)                override { return true; }
		bool supports_mmap()                                     override { return true; }
		bool supports_open(const char *, int)                    override { return true; }
		bool supports_pipe()                                     override { return true; }
		bool supports_readlink(const char *, char *, ::size_t)   override { return true; }
		bool supports_rename(const char *, const char *)         override { return true; }
		bool supports_rmdir(const char *)                        override { return true; }
		bool supports_select(int nfds, fd_set *readfds,
		                     fd_set *writefds,
		                     fd_set *exceptfds,
		                     struct timeval *timeout)            override { return true; }
		bool supports_socket(int domain, int type, int protocol) override { return true; }
		bool supports_stat(const char *)                         override { return true; }
		bool supports_symlink(const char *, const char *)        override { return true; }
		bool supports_unlink(const char *)                       override { return true; }

		Libc::File_descriptor *open(const char *, int, int libc_fd);
		Libc::File_descriptor *open(const char *path, int flags) override
		{
			return open(path, flags, Libc::ANY_FD);
		}

		int     access(char const *, int) override;
		int     close(Libc::File_descriptor *) override;
		int     dup2(Libc::File_descriptor *, Libc::File_descriptor *) override;
		int     fcntl(Libc::File_descriptor *, int, long) override;
		int     fstat(Libc::File_descriptor *, struct stat *) override;
		int     fstatfs(Libc::File_descriptor *, struct statfs *) override;
		int     fsync(Libc::File_descriptor *fd) override;
		int     ftruncate(Libc::File_descriptor *, ::off_t) override;
		ssize_t getdirentries(Libc::File_descriptor *, char *, ::size_t , ::off_t *) override;
		int     ioctl(Libc::File_descriptor *, int , char *) override;
		::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence) override;
		int     mkdir(const char *, mode_t) override;
		int     pipe(Libc::File_descriptor *fd[2]) override;
		ssize_t read(Libc::File_descriptor *, void *, ::size_t) override;
		ssize_t readlink(const char *, char *, ::size_t) override;
		int     rename(const char *, const char *) override;
		int     rmdir(const char *) override;
		int     stat(const char *, struct stat *) override;
		int     symlink(const char *, const char *) override;
		int     unlink(const char *) override;
		ssize_t write(Libc::File_descriptor *, const void *, ::size_t ) override;
		void   *mmap(void *, ::size_t, int, int, Libc::File_descriptor *, ::off_t) override;
		int     munmap(void *, ::size_t) override;

		int getpeername(Libc::File_descriptor*, sockaddr*, socklen_t*) override;
		int getsockname(Libc::File_descriptor*, sockaddr*, socklen_t*) override;
		Libc::File_descriptor* accept(Libc::File_descriptor*, sockaddr*, socklen_t*) override;
		int bind(Libc::File_descriptor*, const sockaddr*, socklen_t) override;
		int connect(Libc::File_descriptor*, const sockaddr*, socklen_t) override;
		int listen(Libc::File_descriptor*, int) override;
		ssize_t recvfrom(Libc::File_descriptor*, void*, size_t, int, sockaddr*, socklen_t*) override;
		ssize_t recv(Libc::File_descriptor*, void*, size_t, int) override;
		ssize_t sendto(Libc::File_descriptor*, const void*, size_t, int, const sockaddr*, socklen_t) override;
		ssize_t send(Libc::File_descriptor*, const void*, size_t, int) override;
		int getsockopt(Libc::File_descriptor*, int, int, void*, socklen_t*) override;
		int setsockopt(Libc::File_descriptor*, int, int, const void*, socklen_t) override;
		int shutdown(Libc::File_descriptor*, int) override;
		Libc::File_descriptor* socket(int, int, int) override;

		int _select(int nfds,
		            fd_set *read_out,  fd_set const &read_in,
		            fd_set *write_out, fd_set const &write_in);

		int select(int nfds, fd_set *readfds, fd_set *writefds,
		           fd_set *exceptfds, struct timeval *timeout) override;
};

#endif
