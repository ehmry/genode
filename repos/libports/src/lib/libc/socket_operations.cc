/*
 * \brief  libc socket operations
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2015-06-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
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
#include "libc_file.h"
#include "libc_errno.h"


namespace Libc { extern char const *config_socket(); }


/***************
 ** Utilities **
 ***************/

namespace {

	using Libc::Errno;

	struct Absolute_path : Vfs::Absolute_path
	{
		Absolute_path() { }

		Absolute_path(char const *path, char const *pwd = 0)
		:
			Vfs::Absolute_path(path, pwd)
		{
			remove_trailing('\n');
		}
	};

	struct Exception { };
	struct New_socket_failed : Exception { };
	struct Address_conversion_failed : Exception { };

	struct Socket_context : Libc::Plugin_context
	{
		Absolute_path path;

		Socket_context(Absolute_path const &path)
		: path(path.base()) { }

		virtual ~Socket_context() { }
	};

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


static int read_sockaddr_in(char const *file_name,
                            int libc_fd, struct sockaddr_in *addr, socklen_t *addrlen)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
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
	memcpy(addr, &saddr, *addrlen);
	*addrlen = sizeof(saddr);

	return 0;
}


/***********************
 ** Address functions **
 ***********************/

extern "C" int _getpeername(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return read_sockaddr_in("remote", libc_fd, (sockaddr_in *)addr, addrlen);
}


extern "C" int getpeername(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return _getpeername(libc_fd, addr, addrlen);
}


extern "C" int _getsockname(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return read_sockaddr_in("local", libc_fd, (sockaddr_in *)addr, addrlen);
}


extern "C" int getsockname(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return _getsockname(libc_fd, addr, addrlen);
}


/**************************
 ** Socket transport API **
 **************************/

extern "C" int _accept(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(EBADF);

	char accept_socket[10];
	{
		Absolute_path file("accept", context->path.base());
		int const fd = open(file.base(), O_RDONLY);
		if (fd == -1) {
			Genode::error(__func__, ": accept file not accessible");
			return Errno(EINVAL);
		}
		int n = 0;
		/* XXX currently reading accept may return without new connection */
		do { n = read(fd, accept_socket, sizeof(accept_socket)); } while (n == 0);
		close(fd);
		if (n == -1 || n >= (int)sizeof(accept_socket) - 1)
			return Errno(EINVAL);

		accept_socket[n] = 0;
	}

	Absolute_path accept_path(accept_socket, Libc::config_socket());
	Socket_context *accept_context = new (Genode::env()->heap())
	                                      Socket_context(accept_path);
	Libc::File_descriptor *accept_fd =
		Libc::file_descriptor_allocator()->alloc(nullptr, accept_context);

	if (addr && addrlen) {
		Absolute_path file("remote", accept_path.base());
		int const fd = open(file.base(), O_RDONLY);
		if (fd == -1) {
			Genode::error(__func__, ": remote file not accessible");
			return Errno(EINVAL);
		}
		Sockaddr_string remote;
		int const n = read(fd, remote.base(), remote.capacity() - 1);
		close(fd);
		if (n == -1 || !n || n >= (int)remote.capacity() - 1)
			return Errno(EINVAL);

		remote.terminate(n);
		remote.remove_trailing_newline();

		sockaddr_in remote_addr = sockaddr_in_struct(remote.host(), remote.port());
		memcpy(addr, &remote_addr, *addrlen);
		*addrlen = sizeof(remote_addr);
	}

	return accept_fd->libc_fd;
}


extern "C" int accept(int libc_fd, struct sockaddr *addr, socklen_t *addrlen)
{
	return _accept(libc_fd, addr, addrlen);
}


extern "C" int _bind(int libc_fd, const struct sockaddr *addr, socklen_t addrlen)
{
	if (addr->sa_family != AF_INET) {
		Genode::error(__func__, ": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(EBADF);

	{
		Sockaddr_string addr_string(host_string(*(sockaddr_in *)addr),
		                            port_string(*(sockaddr_in *)addr));

		Absolute_path file("bind", context->path.base());
		int const fd = open(file.base(), O_WRONLY);
		if (fd == -1) {
			Genode::error(__func__, ": bind file not accessible");
			return Errno(EINVAL);
		}
		int const len = strlen(addr_string.base());
		int const n   = write(fd, addr_string.base(), len);
		close(fd);
		if (n != len) return Errno(EIO);
	}

	return 0;
}


extern "C" int bind(int libc_fd, const struct sockaddr *addr, socklen_t addrlen)
{
	return _bind(libc_fd, addr, addrlen);
}


extern "C" int _connect(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	if (addr->sa_family != AF_INET) {
		Genode::error(__func__, ": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(EBADF);

	{
		Sockaddr_string addr_string(host_string(*(sockaddr_in const *)addr),
		                            port_string(*(sockaddr_in const *)addr));

		Absolute_path file("connect", context->path.base());
		int const fd = open(file.base(), O_WRONLY);
		if (fd == -1) {
			Genode::error(__func__, ": connect file not accessible");
			return Errno(EINVAL);
		}
		int const len = strlen(addr_string.base());
		int const n   = write(fd, addr_string.base(), len);
		close(fd);
		if (n != len) return Errno(EIO);
	}

	return 0;
}


extern "C" int connect(int libc_fd, const struct sockaddr *addr, socklen_t addrlen)
{
	return _connect(libc_fd, addr, addrlen);
}


extern "C" int _listen(int libc_fd, int backlog)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(EBADF);

	{
		Absolute_path file("listen", context->path.base());
		int const fd = open(file.base(), O_WRONLY);
		if (fd == -1) {
			Genode::error(__func__, ": listen file not accessible");
			return Errno(EINVAL);
		}
		char buf[10];
		snprintf(buf, sizeof(buf), "%d", backlog);
		int const n = write(fd, buf, strlen(buf));
		close(fd);
		if ((unsigned)n != strlen(buf)) return Errno(EIO);
	}

	return 0;
}


extern "C" int listen(int libc_fd, int backlog)
{
	return _listen(libc_fd, backlog);
}


extern "C" ssize_t _recvfrom(int libc_fd, void *buf, ::size_t len, int flags,
                              struct sockaddr *src_addr, socklen_t *addrlen)
{
	/* TODO ENOTCONN */

	if (!buf || !len) return Errno(EINVAL);

	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context))) {
		errno = EBADF;
		return -1;
	}
	/* TODO */
	if (src_addr) {
		Genode::error(__func__, ": src_addr parameter not supported yet");
		errno = EINVAL;
		return -1;
	}

	ssize_t out_len = 0;
	{
		Absolute_path file("data", context->path.base());
		int const fd = open(file.base(), O_RDONLY);
		if (fd == -1) {
			Genode::error(__func__, ": data file not accessible");
			errno = EINVAL;
			return -1;
		}
		out_len = read(fd, buf, len);
		close(fd);
	}

	return out_len;
}


extern "C" ssize_t recvfrom(int libc_fd, void *buf, ::size_t len, int flags,
                              struct sockaddr *src_addr, socklen_t *addrlen)
{
	return _recvfrom(libc_fd, buf, len, flags, src_addr, addrlen);
}


extern "C" ssize_t recv(int libc_fd, void *buf, ::size_t len, int flags)
{
	/* identical to recvfrom() with a NULL src_addr argument */
	return recvfrom(libc_fd, buf, len, flags, nullptr, nullptr);
}


extern "C" ssize_t _recvmsg(int libc_fd, struct msghdr *msg, int flags)
{
	PDBG("##########  TODO  ##########");
	return 0;
}


extern "C" ssize_t recvmsg(int libc_fd, struct msghdr *msg, int flags)
{
	return _recvmsg(libc_fd, msg, flags);
}


extern "C" ssize_t _sendto(int libc_fd, const void *buf, ::size_t len, int flags,
                            const struct sockaddr *dest_addr, socklen_t addrlen)
{
	/* TODO ENOTCONN */

	if (!buf || !len) {
		errno = EINVAL;
		return -1;
	}

	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context))) {
		errno = EBADF;
		return -1;
	}
	/* TODO */
	if (dest_addr) {
		Genode::error(__func__, ": dest_addr parameter not supported yet");
		errno = EINVAL;
		return -1;
	}

	ssize_t out_len = 0;
	{
		Absolute_path file("data", context->path.base());
		int const fd = open(file.base(), O_WRONLY);
		if (fd == -1) {
			Genode::error(__func__, ": data file not accessible");
			errno = EINVAL;
			return -1;
		}
		out_len = write(fd, buf, len);
		close(fd);
	}

	return out_len;
}


extern "C" ssize_t sendto(int libc_fd, const void *buf, ::size_t len, int flags,
                            const struct sockaddr *dest_addr, socklen_t addrlen)
{
	return _sendto(libc_fd, buf, len, flags, dest_addr, addrlen);
}


extern "C" ssize_t send(int libc_fd, const void *buf, ::size_t len, int flags)
{
	/* identical to sendto() with a NULL dest_addr argument */
	return sendto(libc_fd, buf, len, flags, nullptr, 0);
}


extern "C" int _getsockopt(int libc_fd, int level, int optname,
                          void *optval, socklen_t *optlen)
{
	PDBG("##########  TODO  ##########");
	return 0;
}


extern "C" int getsockopt(int libc_fd, int level, int optname,
                          void *optval, socklen_t *optlen)
{
	return _getsockopt(libc_fd, level, optname, optval, optlen);
}


extern "C" int _setsockopt(int libc_fd, int level, int optname,
                          const void *optval, socklen_t optlen)
{
	PDBG("##########  TODO  ##########");
	return 0;
}


extern "C" int setsockopt(int libc_fd, int level, int optname,
                          const void *optval, socklen_t optlen)
{
	return _setsockopt(libc_fd, level, optname, optval, optlen);
}


extern "C" int shutdown(int libc_fd, int how)
{
	/* TODO ENOTCONN */

	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context))) {
		errno = EBADF;
		return -1;
	}

	unlink(context->path.base());

	return 0;
}


static void new_tcp_socket(Absolute_path &path)
{
	Absolute_path new_socket("new_socket", path.base());

	int const fd = open(new_socket.base(), O_RDONLY);
	if (fd == -1) {
		Genode::error(__func__, ": new_socket file not accessible - socket fs not mounted?");
		errno = EINVAL;
		throw New_socket_failed();
	}
	char buf[10];
	int const n = read(fd, buf, sizeof(buf));
	close(fd);
	if (n == -1 || !n || n >= (int)sizeof(buf) - 1) {
		errno = EINVAL;
		throw New_socket_failed();
	}
	buf[n] = 0;

	path.append("/");
	path.append(buf);
}

extern "C" int _socket(int domain, int type, int protocol)
{
	Absolute_path path(Libc::config_socket());

	if (path == "") {
		Genode::error(__func__, ": socket fs not mounted");
		errno = EACCES;
		return -1;
	}

	if (type != SOCK_STREAM || (protocol != 0 && protocol != IPPROTO_TCP)) {
		Genode::error(__func__, ": only TCP for now");
		errno = EAFNOSUPPORT;
		return -1;
	}

	try { new_tcp_socket(path); } catch (New_socket_failed) { return -1; }

	Socket_context *context = new (Genode::env()->heap())
	                          Socket_context(path);
	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->alloc(nullptr, context);

	return fd->libc_fd;
}


extern "C" int socket(int domain, int type, int protocol)
{
	return _socket(domain, type, protocol);
}


/*********************
 ** File operations **
 *********************/

bool Libc::socket_close(int libc_fd, int &result)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return false;

	Genode::destroy(Genode::env()->heap(), context);
	Libc::file_descriptor_allocator()->free(fd);

	return true;
}


bool Libc::socket_read(int libc_fd, void *buf, ::size_t count, ::ssize_t &result)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return false;

	result = _recvfrom(libc_fd, buf, count, 0, nullptr, nullptr);

	return true;
}


bool Libc::socket_write(int libc_fd, void const *buf, ::size_t count, ::ssize_t &result)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	Socket_context *context;
	if (!fd || !(context = dynamic_cast<Socket_context *>(fd->context)))
		return false;

	result = _sendto(libc_fd, buf, count, 0, nullptr, 0);

	return true;
}
