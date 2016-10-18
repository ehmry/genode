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
#include <base/thread.h>

/* local includes */
#include <lxip/lxip.h>
#include <lx.h>
#include <nic.h>


static const bool verbose = false;


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


namespace Net
{
	struct Ticker;
		class Socketcall;

		enum Opcode { OP_SOCKET   = 0,  OP_CLOSE  = 1, OP_BIND     = 2,  OP_LISTEN  = 3,
		              OP_ACCEPT   = 4,  OP_POLL   = 5, OP_RECV     = 6,  OP_CONNECT = 7,
		              OP_SEND     = 8,  OP_SETOPT = 9, OP_GETOPT   = 10, OP_GETNAME = 11,
		              OP_PEERNAME = 12, OP_IOCTL = 13, OP_SHUTDOWN = 14 };

		struct Call
		{
			Opcode       opcode;
			Lxip::Handle handle;

			union
			{
				struct
				{
					Lxip::Type type;
				} socket;
				struct
				{
					int backlog;
				} listen;
				struct
				{
					void           *addr;
					Lxip::uint32_t *len;
				} accept;
				struct
				{
					mutable void   *buf;
					Lxip::size_t    len;
					int             flags;
					void           *addr;
					Lxip::uint32_t  addr_len;
				} msg;
				struct
				{
					int             level;
					int             optname;
					const void     *optval;
					Lxip::uint32_t  optlen;
					int            *optlen_ptr;
				} sockopt;
				struct {
					bool block;
				} poll;
				struct {
					int           request;
					unsigned long arg;
				} ioctl;
				struct {
					int how;
				} shutdown;
			};

			struct Linux::sockaddr_storage addr;
			Lxip::uint32_t                 addr_len;
		};

		union Result
		{
			int           err;
			Lxip::ssize_t len;
		};
};


struct Net::Ticker
{
	void (*_tick)()      = nullptr;
	bool _skip_next_tick = false;

	void tick_function(void (*tick)()) { _tick = tick; }

	void skip_next_tick() { _skip_next_tick = true; }

	void tick()
	{
		if (!_skip_next_tick && _tick)
			_tick();

		_skip_next_tick = false;
	}
};


static Net::Ticker *TICKER = nullptr;

extern "C" void TICK() { if (TICKER) TICKER->tick(); }


class Net::Socketcall : public Genode::Signal_dispatcher_base,
                        public Genode::Signal_context_capability,
                        public Lxip::Socketcall,
                        public Genode::Thread_deprecated<64 * 1024 * sizeof(Genode::addr_t)>
{
	private:

		Call         _call;
		Result       _result;
		Lxip::Handle _handle;

		Genode::Signal_receiver    &_sig_rec;
		Genode::Signal_transmitter  _signal;
		Genode::Semaphore           _block;

		Ticker _ticker;

		void _submit_and_block()
		{
			_signal.submit(); /* global submit */
			_block.down();
		}

		void _unblock() { _block.up(); }

		struct Linux::socket * call_socket()
		{
			return static_cast<struct Linux::socket *>(_call.handle.socket);
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
					struct sockaddr_in *out = (struct sockaddr_in *)&_call.addr;

					out->sin_family       = family;
					out->sin_port         = in->sin_port;
					out->sin_addr.s_addr  = in->sin_addr.s_addr;

					return sizeof(struct sockaddr_in);
			}

			return 0;
		}

		/******************************************
		 ** Glue interface to Linux TCP/IP stack **
		 ******************************************/

		void _do_accept()
		{
			using namespace Linux;

			struct socket *sock     = call_socket();
			struct socket *new_sock = sock_alloc();

			_handle.socket = 0;

			if (!new_sock)
				return;

			new_sock->type = sock->type;
			new_sock->ops  = sock->ops;

			int flags = 0;
			if (_call.handle.non_block) flags |= Linux::O_NONBLOCK;

			if (int err = sock->ops->accept(sock, new_sock, flags)) {
				if (err == -EAGAIN) _handle.socket = (void*)0x1;
				kfree(new_sock);
				return;
			}

			_handle.socket = static_cast<void *>(new_sock);

			if (!_call.accept.addr)
				return;

			int len;
			if ((new_sock->ops->getname(new_sock, (struct sockaddr *)&_call.addr,
			    &len, 2)) < 0)
				return;

			*_call.accept.len = min(*_call.accept.len, len);
			Genode::memcpy(_call.accept.addr, &_call.addr, *_call.accept.len);
		}

		void _do_bind()
		{
			struct Linux::socket *sock = call_socket();

			_result.err = sock->ops->bind(sock, (struct Linux::sockaddr *) &_call.addr,
			                              _call.addr_len);
		}

		void _do_close()
		{
			using namespace Linux;

			struct socket *s = call_socket();
			if (s->ops)
				s->ops->release(s);

			kfree(s->wq);
			kfree(s);
		}

		void _do_connect()
		{
			Linux::socket *sock = call_socket();

			//XXX: have a look at the file flags
			_result.err = sock->ops->connect(sock, (struct Linux::sockaddr *) &_call.addr,
			                                 _call.addr_len, 0);
		}

		void _do_getname(int peer)
		{
			int len = sizeof(Linux::sockaddr_storage);
			_result.err = call_socket()->ops->getname(call_socket(),
			                                          (struct Linux::sockaddr *)&_call.addr,
			                                          &len, peer);

			*_call.accept.len = Linux::min(*_call.accept.len, len);
			Genode::memcpy(_call.accept.addr, &_call.addr, *_call.accept.len);
		}


		void _do_getopt()
		{
			_result.err = Linux::sock_getsockopt(call_socket(), _call.sockopt.level,
			                                     _call.sockopt.optname,
			                                     (char *)_call.sockopt.optval,
			                                     _call.sockopt.optlen_ptr);
		}

		void _do_ioctl()
		{
			_result.err = call_socket()->ops->ioctl(call_socket(),
			                                        _call.ioctl.request,
			                                        _call.ioctl.arg);
		}

		void _do_listen()
		{
			_result.err = call_socket()->ops->listen(call_socket(),
			                                         _call.listen.backlog);
		}

		void _do_poll()
		{
			using namespace Linux;
			struct socket *sock = call_socket();
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
			set_sock_wait(sock, _call.poll.block ? 1 : 0);
			int mask = sock->ops->poll(&f, sock, 0);
			set_sock_wait(sock, 0);

			_result.err = 0;
			if (mask & POLLIN_SET)
				_result.err |= Lxip::POLLIN;
			if (mask & POLLOUT_SET)
				_result.err |= Lxip::POLLOUT;
			if (mask & POLLEX_SET)
				_result.err |= Lxip::POLLEX;
		}

		void _do_recv()
		{
			using namespace Linux;
			struct msghdr msg;
			struct iovec  iov;

			msg.msg_name    = _call.msg.addr;
			msg.msg_namelen = _call.msg.addr_len;

			msg.msg_control      = nullptr;
			msg.msg_controllen   = 0;
			msg.msg_iter.iov     = &iov;
			msg.msg_iter.nr_segs = 1;
			msg.msg_iter.count   = _call.msg.len;

			iov.iov_len        = _call.msg.len;
			iov.iov_base       = _call.msg.buf;
			msg.msg_flags      = 0;

			if (_call.handle.non_block)
				msg.msg_flags |= MSG_DONTWAIT;

			//XXX: check for non-blocking flag
			_result.len = call_socket()->ops->recvmsg(call_socket(), &msg,
			                                          _call.msg.len,
			                                          _call.msg.flags);

			_call.msg.addr_len = msg.msg_namelen;
		}

		void _do_send()
		{
			using namespace Linux;
			struct msghdr msg;
			struct iovec  iov;

			_result.len = socket_check_state(call_socket());
			if (_result.len < 0)
				return;

			msg.msg_control      = nullptr;
			msg.msg_controllen   = 0;
			msg.msg_iter.iov     = &iov;
			msg.msg_iter.nr_segs = 1;
			msg.msg_iter.count   = _call.msg.len;

			iov.iov_len        = _call.msg.len;
			iov.iov_base       = _call.msg.buf;
			msg.msg_name       = _call.msg.addr;
			msg.msg_namelen    = _call.msg.addr_len;
			msg.msg_flags      = _call.msg.flags;

			if (_call.handle.non_block)
				msg.msg_flags |= MSG_DONTWAIT;

			_result.len = call_socket()->ops->sendmsg(call_socket(), &msg,
			                                          _call.msg.len);
		}

		void _do_setopt()
		{
			_result.err = Linux::sock_setsockopt(call_socket(), _call.sockopt.level,
			                                     _call.sockopt.optname,
			                                     (char *)_call.sockopt.optval,
			                                     _call.sockopt.optlen);
		}

		void _do_shutdown()
		{
			_result.err = call_socket()->ops->shutdown(call_socket(),
			                                            _call.shutdown.how);
		}

		void _do_socket()
		{
			using namespace Linux;
			int type = _call.socket.type == Lxip::TYPE_STREAM ? SOCK_STREAM  :
			                                                    SOCK_DGRAM;

			struct socket *s = sock_alloc();

			if (sock_create_kern(nullptr, AF_INET, type, 0, &s)) {
				_handle.socket = 0;
				kfree(s);
				return;
			}

			_handle.socket = static_cast<void *>(s);
		}

	public:

		Socketcall(Genode::Signal_receiver &sig_rec)
		:
			Thread_deprecated("socketcall"),
			_sig_rec(sig_rec),
			_signal(Genode::Signal_context_capability(_sig_rec.manage(this)))
		{
			start();
		}

		~Socketcall() { _sig_rec.dissolve(this); }

		void entry()
		{
			while (true) {
				Genode::Signal s = _sig_rec.wait_for_signal();
				static_cast<Genode::Signal_dispatcher_base *>(s.context())->dispatch(s.num());

				_ticker.tick();
			}
		}

		/***********************
		 ** Signal dispatcher **
		 ***********************/

		void dispatch(unsigned num)
		{
			switch (_call.opcode) {

				case OP_ACCEPT   : _do_accept();   break;
				case OP_BIND     : _do_bind();     break;
				case OP_CLOSE    : _do_close();    break;
				case OP_CONNECT  : _do_connect();  break;
				case OP_GETNAME  : _do_getname(0); break;
				case OP_GETOPT   : _do_getopt();   break;
				case OP_IOCTL    : _do_ioctl();    break;
				case OP_PEERNAME : _do_getname(1); break;
				case OP_LISTEN   : _do_listen();   break;
				case OP_POLL     : _do_poll();     break;
				case OP_RECV     : _do_recv();     break;
				case OP_SEND     : _do_send();     break;
				case OP_SETOPT   : _do_setopt();   break;
				case OP_SHUTDOWN : _do_shutdown(); break;
				case OP_SOCKET   : _do_socket();   break;

				default:
					_handle.socket = 0;
					Genode::warning("unkown opcode: ", (int)_call.opcode);
			}

			/* XXX socketcall operations do not tick */
			_ticker.skip_next_tick();

			_unblock();
		}


		/**************************
		 ** Socketcall interface **
		 **************************/

		Lxip::Handle accept(Lxip::Handle h, void *addr, Lxip::uint32_t *len)
		{
			_call.opcode      = OP_ACCEPT;
			_call.handle      = h;
			_call.accept.addr = addr;
			_call.accept.len  = len;

			_submit_and_block();

			return _handle;
		}

		int bind(Lxip::Handle h, Lxip::uint16_t family, void *addr)
		{
			_call.opcode   = OP_BIND;
			_call.handle   = h;
			_call.addr_len = _family_handler(family, addr);

			_submit_and_block();

			return _result.err;
		}

		void close(Lxip::Handle h)
		{
			_call.opcode = OP_CLOSE;
			_call.handle = h;

			_submit_and_block();
		}

		int connect(Lxip::Handle h, Lxip::uint16_t family, void *addr)
		{
			_call.opcode   = OP_CONNECT;
			_call.handle   = h;
			_call.addr_len = _family_handler(family, addr);

			_submit_and_block();

			return _result.err;
		}

		int getpeername(Lxip::Handle h, void *addr, Lxip::uint32_t *len)
		{
			_call.opcode      = OP_PEERNAME;
			_call.handle      = h;
			_call.accept.len  = len;
			_call.accept.addr = addr;

			_submit_and_block();

			return _result.err;
		}

		int getsockname(Lxip::Handle h, void *addr, Lxip::uint32_t *len)
		{
			_call.opcode      = OP_GETNAME;
			_call.handle      = h;
			_call.accept.len  = len;
			_call.accept.addr = addr;

			_submit_and_block();

			return _result.err;
		}

		int getsockopt(Lxip::Handle h, int level, int optname,
		               void *optval, int *optlen)
		{
			_call.opcode             = OP_GETOPT;
			_call.handle             = h;
			_call.sockopt.level      = level;
			_call.sockopt.optname    = optname;
			_call.sockopt.optval     = optval;
			_call.sockopt.optlen_ptr = optlen;

			_submit_and_block();

			return _result.err;
		}

		int ioctl(Lxip::Handle h, int request, char *arg)
		{
			_call.opcode        = OP_IOCTL;
			_call.handle        = h;
			_call.ioctl.request = request;
			_call.ioctl.arg     = (unsigned long)arg;

			_submit_and_block();

			return _result.err;
		}

		int listen(Lxip::Handle h, int backlog)
		{
			_call.opcode         = OP_LISTEN;
			_call.handle         = h;
			_call.listen.backlog = backlog;

			_submit_and_block();

			return _result.err;
		}

		int poll(Lxip::Handle h, bool block)
		{
			_call.opcode     = OP_POLL;
			_call.handle     = h;
			_call.poll.block = block;

			_submit_and_block();

			return _result.err;
		}

		Lxip::ssize_t recv(Lxip::Handle h, void *buf, Lxip::size_t len, int flags,
		                   Lxip::uint16_t family, void *addr,
		                   Lxip::uint32_t *addr_len)
		{
			_call.opcode       = OP_RECV;
			_call.handle       = h;
			_call.msg.buf      = buf;
			_call.msg.len      = len;
			_call.msg.addr     = addr;
			_call.msg.addr_len = addr_len ? *addr_len : 0;
			_call.msg.flags    = flags;
			_call.addr_len     = _family_handler(family, addr);

			_submit_and_block();

			if (addr_len)
				*addr_len = _call.msg.addr_len;

			return _result.len;
		}

		Lxip::ssize_t send(Lxip::Handle h, const void *buf, Lxip::size_t len, int flags,
		                   Lxip::uint16_t family, void *addr)
		{
			_call.opcode        = OP_SEND;
			_call.handle        = h;
			_call.msg.buf       = (void *)buf;
			_call.msg.len       = len;
			_call.msg.flags     = flags;
			_call.msg.addr      = addr;
			_call.msg.addr_len  = addr ? sizeof(Linux::sockaddr_in) : 0;

			_submit_and_block();

			return _result.len;
		}

		int setsockopt(Lxip::Handle h, int level, int optname,
		               const void *optval, Lxip::uint32_t optlen)
		{
			_call.opcode          = OP_SETOPT,
			_call.handle          = h;
			_call.sockopt.level   = level;
			_call.sockopt.optname = optname;
			_call.sockopt.optval  = optval;
			_call.sockopt.optlen  = optlen;

			_submit_and_block();

			return _result.err;
		}

		int shutdown(Lxip::Handle h, int how)
		{
			_call.opcode       = OP_SHUTDOWN;
			_call.handle       = h;
			_call.shutdown.how = how;

			_submit_and_block();

			return _result.err;
		}

		Lxip::Handle socket(Lxip::Type type)
		{
			_call.opcode      = OP_SOCKET;
			_call.socket.type = type;

			_submit_and_block();

			return _handle;
		}


		/*******************
		 ** new interface **
		 *******************/

		void register_ticker(void(*tick)()) { _ticker.tick_function(tick); }

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

			_call.opcode   = OP_BIND;
			_call.handle   = h;
			_call.addr_len = _family_handler(AF_INET, (void*)&in_addr);

			_submit_and_block();

			return _result.err;
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

			_call.opcode   = OP_CONNECT;
			_call.handle   = h;
			_call.addr_len = _family_handler(AF_INET, (void*)&in_addr);

			_submit_and_block();

			return _result.err;
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

			_call.opcode        = OP_IOCTL;
			_call.handle        = h;
			_call.ioctl.request = Lxip::Ioctl_cmd::LINUX_IFADDR;
			_call.ioctl.arg     = (unsigned long)&ifr;

			_submit_and_block();

			if (_result.err < 0) return _result.err;

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
			Lxip::uint32_t s_addr_len = sizeof(s_addr);

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


Lxip::Socketcall & Lxip::init(char const *address_config)
{
	static Genode::Signal_receiver sig_rec;

	Lx::timer_init(sig_rec);
	Lx::event_init(sig_rec);
	Lx::nic_client_init(sig_rec);

	static int init = lxip_init(address_config);
	static Net::Socketcall socketcall(sig_rec);

	return socketcall;
}
