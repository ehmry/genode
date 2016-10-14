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
	struct New_socket_failed : Exception { };
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


static int read_sockaddr_in(char const *file_name, Libc::File_descriptor *fd,
                            struct sockaddr_in *addr, socklen_t *addrlen)
{
	Libc::Vfs_plugin::Socket_context *context;
	if (!fd || !(context = dynamic_cast<Libc::Vfs_plugin::Socket_context *>(fd->context)))
		return Errno(EBADF);

	if (!addr)                     return Errno(EFAULT);
	if (!addrlen || *addrlen <= 0) return Errno(EINVAL);

	Absolute_path file(file_name, context->path.base());
	int const read_fd = open(file.base(), O_RDONLY);
	if (read_fd == -1) {
		Genode::error(__func__, ": ", file_name, " not accessible");
		return Errno(ENOTCONN);
	}
	Sockaddr_string addr_string;
	int const n = read(read_fd, addr_string.base(), addr_string.capacity() - 1);
	close(read_fd);
	if (n == -1 || !n || n >= (int)addr_string.capacity() - 1)
		return Errno(EINVAL);

	addr_string.terminate(n);
	addr_string.remove_trailing_newline();

	/* convert the address but do not exceed the caller's buffer */
	sockaddr_in saddr = sockaddr_in_struct(addr_string.host(), addr_string.port());
	Genode::memcpy(addr, &saddr, *addrlen);
	*addrlen = sizeof(saddr);

	return 0;
}


/***********************
 ** Address functions **
 ***********************/

int Libc::Vfs_plugin::getpeername(Libc::File_descriptor *libc_fd,
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

	Socket_context *context;
	if (!libc_fd || !(context = dynamic_cast<Socket_context *>(libc_fd->context))) {
		Errno(EBADF);
		return 0;
	}

	char accept_socket[24];
	Vfs_handle *accept_handle = context->accept_handle();
	if (!accept_handle) {
		Errno(EINVAL);
		return 0;
	}

	int n = 0;
	/* XXX currently reading accept may return without new connection */
	do {
		n = _read(*accept_handle, accept_socket, sizeof(accept_socket), true);
		if (n == -1 || n >= (int)sizeof(accept_socket) - 1) {
			return nullptr;
		}
	} while (n == 0);

	accept_socket[n] = 0;

	Vfs::Vfs_handle *data_handle;
	Meta_path accept_path(Libc::config_socket());
	accept_path.append_element(accept_socket);
	accept_path.remove_trailing('\n');
	{
		using namespace Vfs;

		Absolute_path file("data", accept_path.base());
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDONLY,
		                       &data_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			Errno(EINVAL);
			throw New_socket_failed();
		}
	}

	Socket_context *accept_context;
	try {
		accept_context = new (_alloc) Socket_context(accept_path, *data_handle);
	 } catch (Genode::Allocator::Out_of_memory) { Errno(ENOMEM); return nullptr; }

	Libc::File_descriptor *accept_fd;
	try {
		accept_fd = Libc::file_descriptor_allocator()->alloc(this, accept_context);
	} catch (Genode::Allocator::Out_of_memory) {
		destroy(_alloc, accept_context);
		Errno(ENOMEM); return nullptr;
	}

	if (addr && addrlen) {
		Absolute_path file("remote", accept_path.base());
		Vfs_handle *remote_handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDONLY,
		                       &remote_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			Errno(EINVAL);
			return nullptr;
		}
		Vfs_handle::Guard guard(remote_handle);

		Sockaddr_string remote;
		int const n = _read(*remote_handle, remote.base(), remote.capacity() - 1, false);
		if (n == -1 || !n || n >= (int)remote.capacity() - 1) {
			return nullptr;
		}

		remote.terminate(n);
		remote.remove_trailing_newline();

		sockaddr_in remote_addr = sockaddr_in_struct(remote.host(), remote.port());
		Genode::memcpy(addr, &remote_addr, *addrlen);
		*addrlen = sizeof(remote_addr);
	}

	return accept_fd;
}


int Libc::Vfs_plugin::bind(Libc::File_descriptor *fd, const struct sockaddr *addr, socklen_t addrlen)
{
	using namespace Vfs;

	if (addr->sa_family != AF_INET) {
		Genode::error(__func__,": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(EBADF);

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
			return Errno(EINVAL);
		}
		Vfs::Vfs_handle::Guard guard(bind_handle);

		int const len = Genode::strlen(addr_string.base());
		int const n   = _write(*bind_handle, addr_string.base(), len);
		if (n == -1)  return -1;
		if (n != len) return Errno(EIO);
	}

	return 0;
}


int Libc::Vfs_plugin::connect(Libc::File_descriptor *fd, sockaddr const *addr, socklen_t addrlen)
{
	using namespace Vfs;

	if (addr->sa_family != AF_INET) {
		Genode::error(__func__,": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(EBADF);

	{
		Sockaddr_string addr_string(host_string(*(sockaddr_in const *)addr),
		                            port_string(*(sockaddr_in const *)addr));

		Absolute_path file("connect", context->path.base());
		Vfs::Vfs_handle *connect_handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_WRONLY,
		                       &connect_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EINVAL);
		}
		Vfs::Vfs_handle::Guard guard(connect_handle);

		int const len = Genode::strlen(addr_string.base());
		int const n   = _write(*connect_handle, addr_string.base(), len);
		if (n == -1)  return -1;
		if (n != len) return Errno(EIO);
	}

	return 0;
}


int Libc::Vfs_plugin::listen(Libc::File_descriptor *fd, int backlog)
{
	using namespace Vfs;

	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(EBADF);

	{
		Absolute_path file("listen", context->path.base());
		Vfs::Vfs_handle *listen_handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_WRONLY,
		                       &listen_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EINVAL);
		}
		Vfs::Vfs_handle::Guard guard(listen_handle);

		char buf[10];
		Genode::snprintf(buf, sizeof(buf), "%d", backlog);
		int const n = _write(*listen_handle, buf, Genode::strlen(buf));
		if (n == -1) return  -1;
		if ((unsigned)n != Genode::strlen(buf)) return Errno(EIO);
	}

	{
		Absolute_path file("accept", context->path.base());
		Vfs_handle *accept_handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDONLY,
		                       &accept_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EINVAL);
		}

		if (Vfs_handle *old_handle = context->accept_handle())
			old_handle->ds().close(old_handle);

		context->accept_handle(accept_handle);
		Genode::log("opened an accept file and associated it with a socket context");
	}

	return 0;
}


ssize_t Libc::Vfs_plugin::recvfrom(Libc::File_descriptor *fd, void *buf, ::size_t len, int flags,
                              struct sockaddr *src_addr, socklen_t *addrlen)
{
	/* TODO ENOTCONN */

	if (!buf || !len) return Errno(EINVAL);

	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context))) {
		return Errno(EBADF);
	}

	if (src_addr) {
		using namespace Vfs;

		Sockaddr_string addr_string;
		Absolute_path file("from", context->path.base());
		Vfs_handle *handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDONLY,
		                       &handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EINVAL);
		}
		Vfs_handle::Guard guard(handle);

		int const n = _read(*handle, addr_string.base(), addr_string.capacity() - 1, true);
		if (n == -1)
			return -1;
		if (!n || n >= (int)addr_string.capacity() - 1) {
			Genode::error("failed to read '", file, "'");
			Errno(EIO);
		}

		addr_string.terminate(n);
		addr_string.remove_trailing_newline();

		/* convert the address but do not exceed the caller's buffer */
		sockaddr_in from_sa = sockaddr_in_struct(addr_string.host(), addr_string.port());
		Genode::memcpy(src_addr, &from_sa, *addrlen);
		*addrlen = sizeof(from_sa);
	}

	return _read(context->handle(), buf, len, true);
}


ssize_t Libc::Vfs_plugin::recv(Libc::File_descriptor *libc_fd, void *buf, ::size_t len, int flags)
{
	/* identical to recvfrom() with a NULL src_addr argument */
	return recvfrom(libc_fd, buf, len, flags, nullptr, nullptr);
}

/*
ssize_t Libc::Vfs_plugin::recvmsg(Libc::File_descriptor *libc_fd, struct msghdr *msg, int flags)
{
	PDBG("##########  TODO  ##########");
	return 0;
}
*/


ssize_t Libc::Vfs_plugin::sendto(Libc::File_descriptor *fd, const void *buf, ::size_t len, int flags,
                            const struct sockaddr *dest_addr, socklen_t addrlen)
{
	/* TODO ENOTCONN */

	if (!buf || !len) {
		return Errno(EINVAL);
	}

	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context))) {
		return Errno(EBADF);
	}

	if (dest_addr) {
		using namespace Vfs;

		Sockaddr_string addr_string(host_string(*(sockaddr_in const *)dest_addr),
		                            port_string(*(sockaddr_in const *)dest_addr));

		Absolute_path file("to", context->path.base());
		Vfs_handle *handle = nullptr;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_WRONLY,
		                       &handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "', socket VFS plugin not loaded?");
			return Errno(EINVAL);
		}
		Vfs_handle::Guard guard(handle);

		int const len = Genode::strlen(addr_string.base());
		int const n   = _write(*handle, addr_string.base(), len);
		if (n != len) return Errno(EIO);
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
	PDBG("##########  TODO  ##########");
	return 0;
}


int Libc::Vfs_plugin::setsockopt(Libc::File_descriptor *fd, int level, int optname,
                          const void *optval, socklen_t optlen)
{
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return  Errno(EBADF);

	if (level == SOL_SOCKET) {	
		switch (optname) {
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
		case SO_RCVTIMEO: warning(__func__," SO_RCVTIMEO"); break;
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
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context))) {
		return Errno(EBADF);
	}

	unlink(context->path.base());

	return 0;
}


Libc::File_descriptor *Libc::Vfs_plugin::socket(int domain, int type, int protocol)
{
	using namespace Vfs;
	Absolute_path path(Libc::config_socket());

	if (path == "") {
		Genode::error(__func__,": socket fs not mounted");
		Errno(EACCES);
		return 0;
	}

	{
		Libc::Absolute_path file(Libc::config_socket());
		/* just check the type for now */
		switch (type) {
			case SOCK_STREAM: file.append_element("tcp"); break;
			case SOCK_DGRAM: file.append_element("udp"); break;
			default:
				Errno(EAFNOSUPPORT);
				return 0;
		}
		file.append_element("new_socket");

		Vfs_handle *handle;
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDONLY,
		                       &handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", file, "'");
			Errno(EINVAL);
			return 0;
		}
		Vfs_handle::Guard guard(handle);

		char buf[24];
		ssize_t const n = _read(*handle, buf, sizeof(buf), false);
		if (n == -1 || !n || n >= (int)sizeof(buf) - 1) {
			Genode::error(__func__,": failed to read '", file, "'");
			return 0;
		}
		buf[n] = 0;

		path.append_element(buf);
		path.remove_trailing('\n');
	}

	Vfs::Vfs_handle *data_handle;
	{
		Absolute_path file("data", path.base());
		switch (_root_dir.open(file.base(), Directory_service::OPEN_MODE_RDONLY,
		                       &data_handle, _alloc)) {
		case Directory_service::OPEN_OK: break;
		default:
			Genode::error(__func__,": failed to open '", path, "', socket VFS plugin not loaded?");
			Errno(EINVAL);
			return 0;
		}
	}

	Socket_context *context;
	try {
		context = new (_alloc) Socket_context(path, *data_handle);
	} catch (Genode::Allocator::Out_of_memory) {
		Genode::error(__func__,": Out_of_memory");
		data_handle->ds().close(data_handle);
		Errno(ENOMEM);
		return 0;
	}

	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->alloc(this, context);

	return fd;
}
