/**
 * \brief  Front-end and glue to IP stack
 * \author Sebastian Sumpf
 * \date   2013-09-26
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/signal.h>
#include <base/log.h>

/* local includes */
#include <lxip/lxip.h>
#include <lx.h>
#include <nic.h>


namespace Linux {
		#include <lx_emul.h>

		#include <lx_emul/extern_c_begin.h>
		#include <linux/socket.h>
		#include <uapi/linux/in.h>
		#include <uapi/linux/if.h>
		extern int sock_setsockopt(struct socket *sock, int level,
		                           int op, char __user *optval,
		                           unsigned int optlen);
		extern int sock_getsockopt(struct socket *sock, int level,
		                           int op, char __user *optval,
		                           int __user *optlen);
		struct socket *sock_alloc(void);
		#include <lx_emul/extern_c_end.h>
}


namespace {

long ascii_to_long(char const *cp)
{
	unsigned long result = 0;
	Genode::size_t ret = Genode::ascii_to_unsigned(cp, result, 10);
	return result;
}


char const *get_port(char const *s)
{
	char const *p = s;
	while (*++p) {
		if (*p == ':')
			return ++p;
	}
	return nullptr;
}


unsigned get_addr(char const *s)
{
	unsigned char to[4];

	char const *p = s;
	for (int i = 0; *p && i < 4; i++) {
		unsigned long result;

		p += Genode::ascii_to_unsigned(p, result, 10);

		if (*p == '.') p++;

		to[i] = result;
	};

	return (to[0]<<0)|(to[1]<<8)|(to[2]<<16)|(to[3]<<24);
}

} /* anonymous helper foo */


namespace Net { class Socketcall; };

class Net::Socketcall : public Lxip::Socketcall
{
	private:

		struct Linux::sockaddr_storage _addr;
		Lxip::uint32_t                 _addr_len;
		int                            _listen_backlog;
		Lxip::Handle                   _handle;

		struct Linux::socket * call_socket(Lxip::Handle &handle)
		{
			return static_cast<struct Linux::socket *>(handle.socket);
		}

		Lxip::uint32_t _family_handler(Lxip::uint16_t family, void *addr)
		{
			using namespace Linux;

			if (!addr)
				return 0;

			switch (family)
			{
				case AF_INET:

					struct sockaddr_in *in  = (struct sockaddr_in *)addr;
					struct sockaddr_in *out = (struct sockaddr_in *)&_addr;

					out->sin_family       = family;
					out->sin_port         = in->sin_port;
					out->sin_addr.s_addr  = in->sin_addr.s_addr;

					return sizeof(struct sockaddr_in);
			}

			return 0;
		}

	public:

		/**************************
		 ** Socketcall interface **
		 **************************/

		Lxip::Handle accept(Lxip::Handle handle, void *addr, Lxip::uint32_t *len)
		{
			using namespace Linux;

			struct socket *sock     = call_socket(handle);
			struct socket *new_sock = sock_alloc();

			_handle.socket = 0;

			if (!new_sock)
				return _handle;

			new_sock->type = sock->type;
			new_sock->ops  = sock->ops;

			int flags = Linux::O_NONBLOCK;

			if (int err = sock->ops->accept(sock, new_sock, flags)) {
				if (err == -EAGAIN) _handle.socket = (void*)0x1;
				kfree(new_sock);
				return _handle;
			}

			_handle.socket = static_cast<void *>(new_sock);

			if (addr) {
				int tmp_len;
				if ((new_sock->ops->getname(new_sock, (struct sockaddr *)&_addr,
				    &tmp_len, 2)) < 0)
					return _handle;

				*len = min(*len, tmp_len);
				Genode::memcpy(addr, &_addr, *len);
			}

			return _handle;
		}

		int bind(Lxip::Handle h, Lxip::uint16_t family, void *addr)
		{
			_addr_len = _family_handler(family, addr);

			struct Linux::socket *sock = call_socket(h);

			return sock->ops->bind(sock, (struct Linux::sockaddr *) &_addr,
			                       _addr_len);
		}

		void close(Lxip::Handle h)
		{
			struct Linux::socket *sock = call_socket(h);

			if (sock->ops)
				sock->ops->release(sock);

			kfree(sock->wq);
			kfree(sock);
		}

		int connect(Lxip::Handle h, Lxip::uint16_t family, void *addr)
		{
			_addr_len = _family_handler(family, addr);

			Linux::socket *sock = call_socket(h);

			//XXX: have a look at the file flags
			return sock->ops->connect(sock, (struct Linux::sockaddr *) &_addr,
			                          _addr_len, 0);
		}

		int getpeername(Lxip::Handle h, void *addr, Genode::int32_t *len)
		{
			Linux::socket *sock = call_socket(h);
			return sock->ops->getname(
				sock, (struct Linux::sockaddr *)addr, len, 1);
		}

		int getsockname(Lxip::Handle h, void *addr, Lxip::uint32_t *len)
		{
			int tmp_len = sizeof(Linux::sockaddr_storage);
			Linux::socket *sock = call_socket(h);
			int err = sock->ops->getname(sock,
			                                       (struct Linux::sockaddr *)&_addr,
			                                       &tmp_len, 0);

			*len = Linux::min(*len, tmp_len);
			Genode::memcpy(addr, &_addr, *len);

			return err;
		}

		int getsockopt(Lxip::Handle h, int level, int optname,
		               void *optval, int *optlen)
		{
			return Linux::sock_getsockopt(call_socket(h), level,
			                              optname, (char *)optval, optlen);
		}

		int ioctl(Lxip::Handle h, int request, char *arg)
		{
			struct Linux::socket *sock = call_socket(h);
			return sock->ops->ioctl(sock, request, (unsigned long)arg);
		}

		int listen(Lxip::Handle h, int backlog)
		{
			_listen_backlog = backlog;
			struct Linux::socket *sock = call_socket(h);
			return sock->ops->listen(sock, backlog);
		}

		int poll(Lxip::Handle h, bool block)
		{
			using namespace Linux;
			struct Linux::socket *sock = call_socket(h);

			enum {
				POLLIN_SET  = (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR),
				POLLOUT_SET = (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR),
				POLLEX_SET  = (POLLPRI)
			};

			/*
			 * Needed by udp_poll because it may check file->f_flags
			 */
			struct file f;
			f.f_flags = 0;

			/*
			 * Set socket wait queue to one so we can block poll in 'tcp_poll -> poll_wait'
			 */
			set_sock_wait(sock, block ? 1 : 0);
			int mask = sock->ops->poll(&f, sock, 0);
			set_sock_wait(sock, 0);

			int result = 0;
			if (mask & POLLIN_SET)
				result |= Lxip::POLLIN;
			if (mask & POLLOUT_SET)
				result |= Lxip::POLLOUT;
			if (mask & POLLEX_SET)
				result |= Lxip::POLLEX;

			return result;
		}

		Lxip::ssize_t recv(Lxip::Handle h, void *buf, Lxip::size_t len, int flags,
		                   Lxip::uint16_t family, void *addr,
		                   Lxip::uint32_t *addr_len)
		{
			using namespace Linux;
			struct msghdr msg;
			struct iovec  iov;

			iov.iov_len        = len;
			iov.iov_base       = buf;

			msg.msg_name       = addr;
			msg.msg_namelen    = addr ? *addr_len : 0;
			msg.msg_control    = nullptr;
			msg.msg_controllen = 0;
			msg.msg_flags      = 0;
			msg.msg_iter.iov     = &iov;
			msg.msg_iter.nr_segs = 1;
			msg.msg_iter.count   = len;

			struct Linux::socket *sock = call_socket(h);
			Lxip::ssize_t res = sock->ops->recvmsg(
				sock, &msg, len, flags|MSG_DONTWAIT);

			if (addr)
				*addr_len = min(*addr_len, msg.msg_namelen);

			return res;
		}

		Lxip::ssize_t send(Lxip::Handle h, const void *buf, Lxip::size_t len, int flags,
		                   Lxip::uint16_t family, void *addr)
		{
			using namespace Linux;
			struct msghdr msg;
			struct iovec  iov;

			struct Linux::socket *sock = call_socket(h);

			if (Lxip::ssize_t r = socket_check_state(sock) < 0)
				return r;

			msg.msg_control      = nullptr;
			msg.msg_controllen   = 0;
			msg.msg_iter.iov     = &iov;
			msg.msg_iter.nr_segs = 1;
			msg.msg_iter.count   = len;

			iov.iov_len        = len;
			iov.iov_base       = (void*)buf;
			msg.msg_name       = _addr_len ? &_addr : 0;
			msg.msg_namelen    = _addr_len;
			msg.msg_flags      = flags | MSG_DONTWAIT;

			return sock->ops->sendmsg(sock, &msg, len);
		}

		int setsockopt(Lxip::Handle h, int level, int optname,
		               const void *optval, Lxip::uint32_t optlen)
		{
			return Linux::sock_setsockopt(
				call_socket(h), level, optname, (char *)optval, optlen);
		}

		int shutdown(Lxip::Handle h, int how)
		{
			struct Linux::socket *sock = call_socket(h);
			return sock->ops->shutdown(sock, how);
		}

		Lxip::Handle socket(Lxip::Type t)
		{
			using namespace Linux;
			int type = t == Lxip::TYPE_STREAM ? SOCK_STREAM  :
			                                       SOCK_DGRAM;

			struct socket *s = sock_alloc();

			if (sock_create_kern(nullptr, AF_INET, type, 0, &s)) {
				_handle.socket = 0;
				kfree(s);
				return _handle;
			}

			_handle.socket = static_cast<void *>(s);

			return _handle;
		}


		/*******************
		 ** new interface **
		 *******************/

		int bind_port(Lxip::Handle h, char const *addr)
		{
			using namespace Linux; /* XXX fix Linux::__be16 in sockaddr_in */

			sockaddr_in in_addr;
			Genode::memset(&in_addr, 0, sizeof(in_addr));

			char const *port = get_port(addr);
			if (!port) return -1;
			unsigned s_addr = get_addr(addr);
			if (s_addr == INADDR_NONE) return -1;

			in_addr.sin_port        = htons(ascii_to_long(port));
			in_addr.sin_addr.s_addr = s_addr;

			_addr_len = _family_handler(AF_INET, (void*)&in_addr);
			Genode::memcpy(&_addr, &in_addr, _addr_len);

			struct Linux::socket *sock = call_socket(h);
			return sock->ops->bind(sock, (struct Linux::sockaddr *) &_addr,
			                       _addr_len);
		}

		int dial(Lxip::Handle h, char const *addr)
		{
			using namespace Linux; /* XXX fix Linux::__be16 in sockaddr_in */

			sockaddr_in in_addr;
			Genode::memset(&in_addr, 0, sizeof(in_addr));

			char const *port = get_port(addr);
			if (!port) return -1;
			unsigned s_addr = get_addr(addr);
			if (s_addr == INADDR_NONE) return -1;

			in_addr.sin_port        = htons(ascii_to_long(port));
			in_addr.sin_addr.s_addr = s_addr;

			_addr_len = _family_handler(AF_INET, (void*)&in_addr);
			Genode::memcpy(&_addr, &in_addr, _addr_len);

			Linux::socket *sock = call_socket(h);
			return sock->ops->connect(sock, (struct Linux::sockaddr *) &_addr,
			                          _addr_len, 0);
		}

		int ifaddr(Lxip::Handle h, char *dst, Lxip::size_t len)
		{
			enum { MAX_IP_LEN = 15 + 1 };
			if (len > MAX_IP_LEN) return -1;

			using namespace Linux;

			ifreq ifr;
			Genode::memset(&ifr, 0, sizeof(ifr));

			Genode::strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
			ifr.ifr_addr.sa_family = AF_INET;

			struct Linux::socket *sock = call_socket(h);
			if (int err = sock->ops->ioctl(sock,
			                               Lxip::Ioctl_cmd::LINUX_IFADDR,
			                               (unsigned long)&ifr) < 0)
				return err;

			in_addr const     addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
			unsigned char const *p = (unsigned char*)&addr.s_addr;
			Genode::snprintf(dst, len, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);

			return 0;
		}

		int local(Lxip::Handle h, char *dst, Lxip::size_t len)
		{
			using namespace Linux;

			sockaddr_in s_addr;
			Genode::memset(&s_addr, 0, sizeof(s_addr));
			Lxip::uint32_t s_addr_len = sizeof(s_addr);

			int res = getsockname(h, &s_addr, &s_addr_len);
			if (res < 0) return res;

			in_addr const   i_addr = s_addr.sin_addr;
			unsigned char const *a = (unsigned char*)&i_addr.s_addr;
			unsigned char const *p = (unsigned char*)&s_addr.sin_port;
			Genode::snprintf(dst, len, "%d.%d.%d.%d:%u",
			                 a[0], a[1], a[2], a[3],
			                 (p[0]<<8)|(p[1]<<0));

			return 0;
		}

		int remote(Lxip::Handle h, char *dst, Lxip::size_t len)
		{
			using namespace Linux;

			sockaddr_in s_addr;
			Genode::memset(&s_addr, 0, sizeof(s_addr));
			Genode::int32_t s_addr_len = sizeof(s_addr);

			int res = getpeername(h, &s_addr, &s_addr_len);
			if (res < 0) return res;

			in_addr const   i_addr = s_addr.sin_addr;
			unsigned char const *a = (unsigned char*)&i_addr.s_addr;
			unsigned char const *p = (unsigned char*)&s_addr.sin_port;
			Genode::snprintf(dst, len, "%d.%d.%d.%d:%u",
			                 a[0], a[1], a[2], a[3],
			                 (p[0]<<8)|(p[1]<<0));

			return 0;
		}
};


Lxip::Socketcall & Lxip::init(Genode::Env &env,
                              Genode::Allocator &alloc,
                              void (*ticker)(),
                              char const *address_config)
{
	Lx::timer_init(env, alloc, ticker);
	Lx::event_init(env, ticker);
	Lx::nic_client_init(env, alloc, ticker);

	static int init = lxip_init(address_config);
	static Net::Socketcall socketcall;

	return socketcall;
}
