/*
 * \brief  libc socket operations
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2015-06-23
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <vfs/types.h>
#include <util/string.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>

/* libc-internal includes */
#include "vfs_plugin.h"
#include "libc_file.h"
#include "libc_errno.h"

namespace Libc { extern char const *config_socket(); }


/***************
 ** Utilities **
 ***************/

namespace {

	using Libc::Errno;
	using Libc::Absolute_path;

	struct Exception { };
	struct Address_conversion_failed : Exception { };

	template <int CAPACITY> class String
	{
		private:

			char _buf[CAPACITY] { 0 };

		public:

			String() { }

			constexpr size_t capacity() { return CAPACITY; }

			char const * base() const { return _buf; }
			char       * base()       { return _buf; }

			void terminate(size_t at) { _buf[at] = 0; }

			void remove_trailing_newline()
			{
				int i = 0;
				while (_buf[i] && _buf[i + 1]) i++;

				if (i > 0 && _buf[i] == '\n')
					_buf[i] = 0;
			}
	};

	typedef String<NI_MAXHOST> Host_string;
	typedef String<NI_MAXSERV> Port_string;

	/*
	 * Both NI_MAXHOST and NI_MAXSERV include the terminating 0, which allows
	 * use to put ':' between host and port on concatenation.
	 */
	struct Sockaddr_string : String<NI_MAXHOST + NI_MAXSERV>
	{
		Sockaddr_string() { }

		Sockaddr_string(Host_string const &host, Port_string const &port)
		{
			char *b = base();
			b = stpcpy(b, host.base());
			b = stpcpy(b, ":");
			b = stpcpy(b, port.base());
		}

		Host_string host() const
		{
			Host_string host;

			strncpy(host.base(), base(), host.capacity());
			char *at = strstr(host.base(), ":");
			if (!at)
				throw Address_conversion_failed();
			*at = 0;

			return host;
		}

		Port_string port() const
		{
			Port_string port;

			char *at = strstr(base(), ":");
			if (!at)
				throw Address_conversion_failed();

			strncpy(port.base(), ++at, port.capacity());

			return port;
		}
	};
}


/* TODO move to C++ structs or something */

static Port_string port_string(sockaddr_in const &addr)
{
	Port_string port;

	if (getnameinfo((sockaddr *)&addr, sizeof(addr),
	                nullptr, 0, /* no host conversion */
	                port.base(), port.capacity(),
	                NI_NUMERICHOST | NI_NUMERICSERV) != 0)
		throw Address_conversion_failed();

	return port;
}


static Host_string host_string(sockaddr_in const &addr)
{
	Host_string host;

	if (getnameinfo((sockaddr *)&addr, sizeof(addr),
	                host.base(), host.capacity(),
	                nullptr, 0, /* no port conversion */
	                NI_NUMERICHOST | NI_NUMERICSERV) != 0)
		throw Address_conversion_failed();

	return host;
}


static sockaddr_in sockaddr_in_struct(Host_string const &host, Port_string const &port)
{
	addrinfo hints;
	addrinfo *info = nullptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

	if (getaddrinfo(host.base(), port.base(), &hints, &info))
		throw Address_conversion_failed();

	sockaddr_in addr = *(sockaddr_in*)info->ai_addr;

	freeaddrinfo(info);

	return addr;
}


int Libc::Vfs_plugin::read_sockaddr_in(char const *file_name, Libc::File_descriptor *fd,
                                       struct sockaddr_in *addr, socklen_t *addrlen)
{
	using namespace Vfs;

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	if (!addr)                     return Errno(EFAULT);
	if (!addrlen || *addrlen <= 0) return Errno(EINVAL);

	Genode::Lock::Guard vfs_guard(_lock);

	Absolute_path file(file_name, context->path.base());
	Context *tmp_ctx = _open(file.base(), Directory_service::OPEN_MODE_RDONLY);
	if (tmp_ctx == nullptr) {
		Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
		return -1;
	}
	Context::Guard guard(_alloc, tmp_ctx);

	Sockaddr_string addr_string;
	/* non-blocking read, do not wait for a connection or incoming packet */
	auto const n = _read(*tmp_ctx, addr_string.base(), addr_string.capacity() - 1, false);
	if (!n) /* probably a UDP socket */
		return Errno(ENOTCONN);
	if (n == -1)
		return -1;

	addr_string.terminate(n);
	addr_string.remove_trailing_newline();

	/* convert the address but do not exceed the caller's buffer */
	sockaddr_in saddr = sockaddr_in_struct(addr_string.host(), addr_string.port());
	*addrlen = min(*addrlen, sizeof(saddr));
	Genode::memcpy(addr, &saddr, *addrlen);

	return 0;
}


/***********************
 ** Address functions **
 ***********************/

int Libc::Vfs_plugin::getpeername(File_descriptor *libc_fd,
                                  struct sockaddr *addr,
                                  socklen_t *addrlen)
{
	return read_sockaddr_in("remote", libc_fd, (sockaddr_in *)addr, addrlen);
}


int Libc::Vfs_plugin::getsockname(File_descriptor *libc_fd,
                                  struct sockaddr *addr,
                                  socklen_t *addrlen)
{
	return read_sockaddr_in("local", libc_fd, (sockaddr_in *)addr, addrlen);
}


/**************************
 ** Socket transport API **
 **************************/

Libc::File_descriptor *Libc::Vfs_plugin::accept(File_descriptor *libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	using namespace Vfs;

	Genode::Lock::Guard vfs_guard(_lock);

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(libc_fd->context))) {
		Errno(ENOTSOCK);
		return nullptr;
	}

	Meta_path accept_path(Libc::config_socket());
	{
		char accept_socket[24];

		int n = _read(*context, accept_socket, sizeof(accept_socket)-1, true);
		if (n == -1 || n >= (int)sizeof(accept_socket) - 1)
			return nullptr;

		accept_socket[n] = '\0';

		accept_path.append_element(accept_socket);
		accept_path.remove_trailing('\n');
	}

	Vfs::Vfs_handle *data_handle;
	{
		Absolute_path file("data", accept_path.base());
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDWR,
		                       &data_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			Errno(EIO);
			return nullptr;
		}
	}

	Socket_context *accepted_context = nullptr;
	Libc::File_descriptor *accepted_fd = nullptr;
	try {
		accepted_context = new (_alloc) Socket_context(accept_path, *data_handle);
		accepted_fd = Libc::file_descriptor_allocator()->alloc(this, accepted_context);
	 } catch (Genode::Allocator::Out_of_memory) {
		if (accepted_context)
			destroy(_alloc, accepted_fd);
		else
			data_handle->ds().close(data_handle);
		Errno(ENOMEM);
		return nullptr;
	}

	if (addr && addrlen) {
		Absolute_path file("remote", accept_path.base());
		Context *remote_ctx = _open(file.base(), Directory_service::OPEN_MODE_RDONLY);
		if (remote_ctx == nullptr) {
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			Errno(EIO);
			return nullptr;
		}
		Context::Guard guard(_alloc, remote_ctx);

		Sockaddr_string remote;
		int const n = _read(*remote_ctx, remote.base(), remote.capacity() - 1, false);
		if (n == -1 || !n || n >= (int)remote.capacity() - 1) {
			return nullptr;
		}

		remote.terminate(n);
		remote.remove_trailing_newline();

		sockaddr_in remote_addr = sockaddr_in_struct(remote.host(), remote.port());
		*addrlen = min(*addrlen, sizeof(remote_addr));
		Genode::memcpy(addr, &remote_addr, *addrlen);
	}

	return accepted_fd;
}


int Libc::Vfs_plugin::bind(Libc::File_descriptor *fd, const struct sockaddr *addr, socklen_t addrlen)
{
	using namespace Vfs;

	/* TODO: IPv6 */
	if (addr->sa_family != AF_INET) {
		Genode::error(__func__,": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Genode::Lock::Guard vfs_guard(_lock);

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	{
		Sockaddr_string addr_string(host_string(*(sockaddr_in *)addr),
		                            port_string(*(sockaddr_in *)addr));

		Absolute_path file("bind", context->path.base());
		Vfs::Vfs_handle *bind_handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_WRONLY,
		                       &bind_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EACCES);
		}
		Vfs::Vfs_handle::Guard guard(bind_handle);

		int const len = Genode::strlen(addr_string.base());
		int const n   = _write(*bind_handle, addr_string.base(), len);
		if (n != len)
			return (n == -1) ? -1 : Errno(EACCES);
	}

	return 0;
}


int Libc::Vfs_plugin::connect(Libc::File_descriptor *fd, sockaddr const *addr, socklen_t addrlen)
{
	PDBG("<- this function is racy");
	/* TODO: NONBLOCK */

	using namespace Vfs;

	/* TODO: IPv6 */
	if (addr->sa_family != AF_INET) {
		Genode::error(__func__,": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Genode::Lock::Guard vfs_guard(_lock);

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	/* clear notifications (there should be none) */
	context->reset();

	{
		Sockaddr_string addr_string(host_string(*(sockaddr_in const *)addr),
		                            port_string(*(sockaddr_in const *)addr));

		Absolute_path file("connect", context->path.base());
		Context *tmp_ctx = _open(file.base(), Directory_service::OPEN_MODE_WRONLY);
		if (tmp_ctx == nullptr) {
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return -1;
		}
		Context::Guard guard(_alloc, tmp_ctx);

		int const len = Genode::strlen(addr_string.base());
		int const n   = _write(tmp_ctx->handle(), addr_string.base(), len);
		if (n != len)
			return (n == -1) ? -1 : Errno(EACCES);

		/*
		 * return if a notification came in during write,
		 * otherwise block
		 */
		if (!context->notifications()) {
			PDBG("block for data notification");
			/* block for notification on the data file (the socket context) */
			Task &task = Libc::this_task();
			Task_resume_callback notify_cb(task);
			notify_cb.add_context(*context);
			_yield_vfs(task);
		} else
			PDBG("connect already notified");
	}
	context->ack();

	return 0;
}


int Libc::Vfs_plugin::listen(Libc::File_descriptor *fd, int backlog)
{
	using namespace Vfs;

	Genode::Lock::Guard vfs_guard(_lock);

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	/* set the socket backlog and the socket to listening mode */
	{
		Absolute_path file("listen", context->path.base());
		Vfs::Vfs_handle *listen_handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_WRONLY,
		                       &listen_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EOPNOTSUPP);
		}
		Vfs::Vfs_handle::Guard guard(listen_handle);

		char buf[10];
		int len = Genode::snprintf(buf, sizeof(buf), "%d", backlog);
		int   n = _write(*listen_handle, buf, len);
		if (n != len)
			return (n == -1) ? -1 : Errno(EOPNOTSUPP);
	}

	/* replace the associated data file with the accept file */
	{
		Absolute_path file("accept", context->path.base());
		Vfs_handle *accept_handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDONLY,
		                       &accept_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EOPNOTSUPP);
		}

		Vfs::Vfs_handle &data_handle = context->handle();
		context->handle(*accept_handle);
		data_handle.ds().close(&data_handle);
	}

	/* suscribe to notifications for select */
	context->subscribe();

	return 0;
}


ssize_t Libc::Vfs_plugin::recvfrom(Libc::File_descriptor *fd, void *buf, ::size_t len, int flags,
                                   struct sockaddr *src_addr, socklen_t *addrlen)
{
	/* TODO: NONBLOCK */

	if (!buf || !len) return Errno(EINVAL);

	Genode::Lock::Guard vfs_guard(_lock);

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	/*
	 * TODO: if the read of the 'from' file is QUEUED then immediately
	 * read the data file and block until both callbacks complete
	 */

	if (src_addr) {
		using namespace Vfs;

		if (!context->notifications()) {
			PDBG("blocking for data before reading remote");
			Task &task = Libc::this_task();
			Task_resume_callback notify_cb(task);
			notify_cb.add_context(*context);
			_yield_vfs(task);
		}

		Sockaddr_string addr_string;
		Absolute_path file("remote", context->path.base());
		Context *remote_ctx = _open(file.base(), Directory_service::OPEN_MODE_RDONLY);
		if (remote_ctx == nullptr) {
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return -1;
		}
		Context::Guard guard(_alloc, remote_ctx);

		int const n = _read(*remote_ctx, addr_string.base(), addr_string.capacity() - 1, true);
		PDBG("read remote returned");
		if (!n) /* connection closed */
			return 0;

		if (!n || n >= (int)addr_string.capacity() - 1) {
			Genode::error("failed to read '", file, "'");
			Errno(EIO);
		}

		addr_string.terminate(n);
		addr_string.remove_trailing_newline();

		/* convert the address but do not exceed the caller's buffer */
		sockaddr_in from_sa = sockaddr_in_struct(addr_string.host(), addr_string.port());
		*addrlen = min(*addrlen, sizeof(from_sa));
		Genode::memcpy(src_addr, &from_sa, *addrlen);
	}

	return _read(*context, buf, len, true);
}


ssize_t Libc::Vfs_plugin::recv(Libc::File_descriptor *libc_fd, void *buf, ::size_t len, int flags)
{
	/* identical to recvfrom() with a NULL src_addr argument */
	return recvfrom(libc_fd, buf, len, flags, nullptr, nullptr);
}


ssize_t Libc::Vfs_plugin::sendto(Libc::File_descriptor *fd, const void *buf, ::size_t len, int flags,
                            const struct sockaddr *dest_addr, socklen_t addrlen)
{
	if (!buf || !len) {
		return Errno(EINVAL);
	}

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	Genode::Lock::Guard vfs_guard(_lock);

	/*
	 * TODO: if the write to the 'to' file is QUEUED then immediately
	 * write the data file and block until both callbacks complete
	 */

	if (dest_addr) {
		using namespace Vfs;

		Sockaddr_string addr_string(host_string(*(sockaddr_in const *)dest_addr),
		                            port_string(*(sockaddr_in const *)dest_addr));

		Absolute_path file("remote", context->path.base());
		Vfs_handle *handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_WRONLY,
		                       &handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		case Directory_service::OPEN_ERR_NO_PERM:
			return Errno(EISCONN);
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EIO);
		}
		Vfs_handle::Guard guard(handle);

		int const len = Genode::strlen(addr_string.base());
		int const n   = _write(*handle, addr_string.base(), len);
		if (n != len)
			return (n == -1) ? -1 : Errno(EIO);
	}

	return _write(context->handle(), buf, len);
}


ssize_t Libc::Vfs_plugin::send(Libc::File_descriptor *libc_fd, const void *buf, ::size_t len, int flags)
{
	/* identical to sendto() with a NULL dest_addr argument */
	return sendto(libc_fd, buf, len, flags, nullptr, 0);
}


int Libc::Vfs_plugin::getsockopt(File_descriptor *libc_fd, int level,
		                       int optname, void *optval,
		                       socklen_t *optlen)
{
	Genode::warning("##########  TODO ",__func__," ##########");
	return 0;
}


int Libc::Vfs_plugin::setsockopt(Libc::File_descriptor *fd, int level, int optname,
                          const void *optval, socklen_t optlen)
{
	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	Genode::Lock::Guard vfs_guard(_lock);

	if (level == SOL_SOCKET) {	
		switch (optname) {
		case SO_RCVTIMEO:
			if (optval && optlen >= sizeof(timeval))
				context->timeout(*((struct timeval*)optval));
			warning(__func__," SO_RCVTIMEO, setting timeout");
			break;

		case SO_DEBUG: warning(__func__," SO_DEBUG"); break;
		case SO_REUSEADDR: warning(__func__," SO_REUSEADDR"); break;
		case SO_REUSEPORT: warning(__func__," SO_REUSEPORT"); break;
		case SO_KEEPALIVE: warning(__func__," SO_KEEPALIVE"); break;
		case SO_DONTROUTE: warning(__func__," SO_DONTROUTE"); break;
		case SO_LINGER: warning(__func__," SO_LINGER"); break;
		case SO_BROADCAST: warning(__func__," SO_BROADCAST"); break;
		case SO_OOBINLINE: warning(__func__," SO_OOBINLINE"); break;
		case SO_SNDBUF: warning(__func__," SO_SNDBUF"); break;
		case SO_RCVBUF: warning(__func__," SO_RCVBUF"); break;
		case SO_SNDLOWAT: warning(__func__," SO_SNDLOWAT"); break;
		case SO_RCVLOWAT: warning(__func__," SO_RCVLOWAT"); break;
		case SO_SNDTIMEO: warning(__func__," SO_SNDTIMEO"); break;
		case SO_ACCEPTFILTER: warning(__func__," SO_ACCEPTFILTER"); break;
		case SO_NOSIGPIPE: warning(__func__," SO_NOSIGPIPE"); break;
		case SO_TIMESTAMP: warning(__func__," SO_TIMESTAMP"); break;
		case SO_BINTIME: warning(__func__," SO_BINTIME"); break;
		case SO_ACCEPTCONN: warning(__func__," SO_ACCEPTCONN"); break;
		case SO_TYPE: warning(__func__," SO_TYPE"); break;
		//case SO_PROTOCOL: warning(__func__," SO_PROTOCOL"); break;
		//case SO_PROTOTYPE: warning(__func__," SO_PROTOTYPE"); break;
		case SO_ERROR: warning(__func__," SO_ERROR"); break;
		case SO_SETFIB: warning(__func__," SO_SETFIB"); break;
		}
	}

	return 0;
}


int Libc::Vfs_plugin::shutdown(Libc::File_descriptor *fd, int how)
{
	/* TODO ENOTCONN */

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	unlink(context->path.base());

	return 0;
}


Libc::File_descriptor *Libc::Vfs_plugin::socket(int domain, int type, int protocol)
{
	using namespace Vfs;

	/* eventual path to the new socket data file*/
	Absolute_path path(Libc::config_socket());

	if (path == "") {
		Genode::error(__func__,": socket fs not mounted");
		Errno(EACCES);
		return 0;
	}

	Genode::Lock::Guard vfs_guard(_lock);

	/* lookup the and read 'new_socket' in the appropriate protocol dir */
	{
		Libc::Absolute_path file(Libc::config_socket());

		/* just check the type for now */
		switch (type) {
			case SOCK_STREAM: file.append_element("tcp"); break;
			case SOCK_DGRAM:  file.append_element("udp"); break;
			default:
				Errno(EAFNOSUPPORT);
				return 0;
		}
		file.append_element("new_socket");

		Context *meta_ctx = _open(file.base(), Directory_service::OPEN_MODE_RDONLY);
		if (meta_ctx == nullptr) {
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return nullptr;
		}
		Context::Guard guard(_alloc, meta_ctx);

		char buf[24];
		ssize_t const n = _read(*meta_ctx, buf, sizeof(buf)-1, false);
		if (n == -1 || !n || n >= (int)sizeof(buf) - 1) {
			Genode::error(__func__,": failed to read '", file, "', read returned ",n);
			return 0;
		}
		buf[n] = '\0';

		path.append_element(buf);
		path.remove_trailing('\n');
	}

	Vfs::Vfs_handle *data_handle;
	{
		Absolute_path file("data", path.base());
		if (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDWR,
		                       &data_handle, _alloc) != Directory_service::OPEN_OK ) {
			Genode::error(__func__,": failed to open '", path, "', socket VFS plugin not loaded?");
			Errno(EACCES);
			return nullptr;
		}
	}

	Socket_context *context = nullptr;
	Libc::File_descriptor *fd;
	try {
		context = new (_alloc) Socket_context(path, *data_handle);
		fd = Libc::file_descriptor_allocator()->alloc(this, context);
	} catch (Genode::Allocator::Out_of_memory) {
		if (context)
			destroy(_alloc, context);
		else
			data_handle->ds().close(data_handle);
		Errno(ENOMEM);
		return nullptr;
	}

	/* sockets need notifications to work */
	if (!context->subscribe()) {
		close(fd);
		Errno(EACCES);
		return nullptr;
	}

	return fd;
}
