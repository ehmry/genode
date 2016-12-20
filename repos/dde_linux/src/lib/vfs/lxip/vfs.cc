/*
 * \brief  lxip-based socket file system
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/snprintf.h>
#include <util/string.h>
#include <util/xml_node.h>
#include <vfs/directory_service.h>
#include <vfs/file_io_service.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>
#include <base/debug.h>

/* Lxip includes */
#include <lxip/lxip.h>
#include <lx.h>

extern "C" void wait_for_continue(void);

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

int remove_port(char *p)
{
	long tmp = -1;

	while (*++p)
		if (*p == ':') {
			*p++ = '\0';
			Genode::ascii_to_unsigned(p, tmp, 10);
			break;
		}

	return tmp;
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

}


namespace Lxip {

	using namespace Linux;

	struct Protocol_dir;
	struct Socket_dir;

	class Protocol_dir_impl;

	enum {
		MAX_SOCKETS         = 128,      /* 3 */
		MAX_SOCKET_NAME_LEN = 3 + 1,    /* + \0 */
		MAX_FD_STR_LEN      = 3 + 1 +1, /* + \n + \0 */
		MAX_DATA_LEN        = 32,       /* 255.255.255.255:65536 + something */
	};
}


struct Lxip::Protocol_dir
{
	enum Type { TYPE_STREAM, TYPE_DGRAM };

	virtual char const *top_dir() = 0;
	virtual Type type() = 0;
	virtual unsigned adopt_socket(struct Linux::socket&, bool) = 0;
	virtual bool     lookup_port(long) = 0;
};


struct Lxip::Socket_dir
{
	virtual Protocol_dir &parent() = 0;
	virtual char const *top_dir() = 0;
	virtual unsigned accept(struct Linux::socket&) = 0;
	virtual void     bind(bool) = 0; /* bind to port */
	virtual long     bind() = 0; /* return bound port */
	virtual bool     lookup_port(long) = 0;
	virtual void     connect(bool) = 0;
	virtual void     listen(bool) = 0;
	virtual sockaddr_storage &to_addr() = 0;
	virtual void     close() = 0;
	virtual bool     closed() = 0;
};


namespace Vfs {
	struct Node;
	struct Directory;
	struct File;

	class Lxip_file;
	class Lxip_data_file;
	class Lxip_bind_file;
	class Lxip_accept_file;
	class Lxip_connect_file;
	class Lxip_listen_file;
	class Lxip_local_file;
	class Lxip_remote_file;
	class Lxip_socket_dir;

	class Lxip_new_socket_file;

	class Lxip_vfs_handle;
	class Lxip_file_system;

	typedef Genode::List<Lxip_vfs_handle> Lxip_vfs_handles;
}


/***************
 ** Vfs nodes **
 ***************/

struct Vfs::Lxip_vfs_handle final : Vfs::Vfs_handle, Lxip_vfs_handles::Element
{
	Vfs::File &file;

	Lxip_vfs_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                Vfs::File &file)
	: Vfs_handle(fs, fs, alloc, status_flags), file(file) { }
};


struct Vfs::Node
{
	char const *_name;

	Node(char const *name) : _name(name) { }

	virtual ~Node() { }

	virtual char const *name() { return _name; }
};


struct Vfs::File : Vfs::Node
{

	File(char const *name) : Node(name) { }

	virtual ~File() { }

	/**
	 * Pass len data to handle read callback
	 */
	virtual void read(Lxip_vfs_handle &handle, file_size len)
	{
		Genode::error("LxIP, read to write only handle");
		handle.read_status(Callback::ERR_INVALID);
	}

	/**
	 * Pass len data to handle write callback
	 */
	virtual void write(Lxip_vfs_handle &handle, file_size len)
	{
		Genode::error("LxIP, write to read only handle");
		handle.write_status(Callback::ERR_INVALID);
	}

	/**
	 * Check for data to read or write
	 */
	virtual bool poll() { return false; }
};


/**
 * List of open handles to potentially poll
 *
 * Could be a dynamic queue, but this works for now.
 */
static Vfs::Lxip_vfs_handles _polling_handles;

static void poll_all()
{
	using namespace Linux;

	for (Vfs::Lxip_vfs_handle *h = _polling_handles.first(); h; h = h->next())
		if (h->file.poll())
			h->notify_callback();
}


struct Vfs::Directory : Vfs::Node
{
	Directory(char const *name) : Node(name) { }

	virtual ~Directory() { };

	virtual Vfs::Node *child(char const *name) = 0;
	virtual void dirent(file_offset index, Directory_service::Dirent &dirent) { }
	virtual file_size num_dirent() { return 0; }
};


/*****************************
 ** Lxip vfs specific nodes **
 *****************************/

class Vfs::Lxip_file : public Vfs::File
{
	protected:

		Lxip::Socket_dir &_parent;
		Linux::socket    &_sock;

		char _content_buffer[Lxip::MAX_DATA_LEN];

	public:

		Lxip_file(Lxip::Socket_dir &p, struct Linux::socket &s, char const *name)
		: Vfs::File(name), _parent(p), _sock(s) { }

		virtual ~Lxip_file() { }
};


namespace Lxip {

	enum { DATA_BUFFER_SIZE = 4096 };
	static char *data_buffer()
	{
		static char buf[DATA_BUFFER_SIZE];
		return buf;
	}
}


class Vfs::Lxip_data_file : public Vfs::Lxip_file
{
	public:

		Lxip_data_file(Lxip::Socket_dir &p, struct Linux::socket &s)
		: Lxip_file(p, s, "data") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override
		{
			using namespace Linux;

			struct file f;
			f.f_flags = 0;

			return (_sock.state == SS_CONNECTED) ?
				_sock.ops->poll(&f, &_sock, nullptr) & (POLLIN) : false;
		}

		void write(Lxip_vfs_handle &handle, file_size len) override
		{
			using namespace Linux;

			struct msghdr   msg;
			struct iovec    iov;

			msg.msg_name    = &_parent.to_addr();
			msg.msg_namelen = sizeof(sockaddr_in);
			msg.msg_iter.iov_offset = 0;
			msg.msg_iter.iov = &iov;
			msg.msg_iter.nr_segs = 1;
			msg.msg_control    = nullptr;
			msg.msg_controllen = 0;
			msg.msg_iocb       = nullptr;
			msg.msg_flags      = 0;

			iov.iov_base = (void*)Lxip::data_buffer();
			iov.iov_len  = Lxip::DATA_BUFFER_SIZE;

			file_size remain = len;
			while (remain) {
				int n = min(remain, /*min(sndbuf,*/ Lxip::DATA_BUFFER_SIZE/*)*/);
				msg.msg_iter.count = n =
					handle.write_callback(Lxip::data_buffer(), n, Callback::PARTIAL);
				if (!n) break;

				int res = _sock.ops->sendmsg(&_sock, &msg, n);
				if (res != n) {
					if (res < 0) {
						Genode::error("LxIP send error ", res);
						return handle.write_status(Callback::ERR_IO);
					} else if (res == 0) {
						return handle.write_status(Callback::ERR_TERMINATED);
					} else {
						Genode::error("short write to lxIP socket ",res,"/",n,", returning error");
						return handle.write_status(Callback::ERR_IO);
					}
				}

				remain -= res;
			}

			handle.write_status(Callback::COMPLETE);
		}

		void read(Lxip_vfs_handle &handle, file_size len) override
		{
			using namespace Linux;

			if (_sock.state == SS_UNCONNECTED)
				return handle.write_status(Callback::ERR_TERMINATED);

			struct msghdr   msg;
			struct iovec    iov;

			msg.msg_name       = nullptr;
			msg.msg_namelen    = 0;
			msg.msg_iter.type  = 0;
			msg.msg_iter.iov_offset = 0;
			msg.msg_iter.iov   = &iov;
			msg.msg_iter.nr_segs = 1;
			msg.msg_control    = nullptr;
			msg.msg_controllen = 0;
			msg.msg_iocb       = nullptr;

			iov.iov_base = (void*)Lxip::data_buffer();
			iov.iov_len  = Lxip::DATA_BUFFER_SIZE;

			file_size remain = len;
			while (remain) {
				int const n = msg.msg_iter.count =
					min(remain, Lxip::DATA_BUFFER_SIZE);

				int const res = _sock.ops->recvmsg(&_sock, &msg, n, MSG_DONTWAIT);
				if (res < 0) {
					if (res == Lxip::Io_result::LINUX_EAGAIN) {
						/* no problem, just nothing to read right now */
						return handle.read_status(Callback::COMPLETE);
					} else {
						Genode::error("LxIP recv error ", res);
						return handle.read_status(Callback::ERR_IO);
					}
				} else if (res > 0) {
					/* short read means success */
					handle.read_callback(Lxip::data_buffer(), res, Callback::COMPLETE);
					return;
				} else {
					/* a zero read from Linux means the socket is disconnected */
					if (remain != len) {
						/* recvmsg returned zero but we have some good data */
						handle.read_callback(Lxip::data_buffer(), res, Callback::COMPLETE);
					} else {
						/* recvmsg returned zero but we have nothing */
						_parent.close();
						handle.read_status(Callback::ERR_TERMINATED);
					}
					return;
				}
				remain -= res;

				/* notify the callback if the operation is complete */
				Callback::Status s = remain ?
					Callback::PARTIAL : Callback::COMPLETE;
				handle.read_callback(_content_buffer, res, s);
			}
		}
};


class Vfs::Lxip_bind_file : public Vfs::Lxip_file
{
	private:

		long _port = -1;

	public:

		Lxip_bind_file(Lxip::Socket_dir &p, struct Linux::socket &s)
		: Lxip_file(p, s, "bind") { }

		long port() { return _port; }


		/********************
		 ** File interface **
		 ********************/

		void write(Lxip_vfs_handle &handle, file_size len) override
		{
			using namespace Linux;

			if (handle.seek() != 0)
				return handle.write_status(Callback::ERR_INVALID);
			if (len > (sizeof(_content_buffer) - 2))
				return handle.write_status(Callback::ERR_INVALID);

			/* already bound to port */
			if (_port >= 0) {
				return handle.write_status(Callback::ERR_INVALID);
			}

			len = handle.write_callback(
				_content_buffer, len, Callback::PARTIAL);

			struct sockaddr_storage addr_storage;
			sockaddr_in *addr = (sockaddr_in*)&addr_storage;
			long tmp = remove_port(_content_buffer);
			addr->sin_port = htons(tmp);
			addr->sin_addr.s_addr = get_addr(_content_buffer);
			addr->sin_family = AF_INET;

			int res = _sock.ops->bind(&_sock, (sockaddr*)addr, sizeof(addr_storage));
			if (res != 0) {
				Genode::error("lxIP: bind failed, error ",res);
				return handle.write_status(Callback::ERR_IO);
			}

			_port = tmp;

			_content_buffer[len+0] = '\n';
			_content_buffer[len+1] = '\0';

			_parent.bind(true);
			handle.write_status(Callback::COMPLETE);
		}

		void read(Lxip_vfs_handle &handle, file_size len) override
		{
			if (handle.seek() != 0)            return handle.read_status(Callback::ERR_IO);
			if (len < sizeof(_content_buffer)) return handle.read_status(Callback::ERR_IO);

			Genode::size_t const n = Genode::strlen(_content_buffer);
			handle.read_callback(_content_buffer, n, Callback::COMPLETE);
		}
};


class Vfs::Lxip_listen_file : public Vfs::Lxip_file
{
	public:

		Lxip_listen_file(Lxip::Socket_dir &p, struct Linux::socket &s)
		: Lxip_file(p, s, "listen") { }

		/********************
		 ** File interface **
		 ********************/

		void write(Lxip_vfs_handle &handle, file_size len) override
		{
			if (handle.seek() != 0)
				return handle.write_status(Callback::ERR_INVALID);
			if (len > (sizeof(_content_buffer) - 2))
				return handle.write_status(Callback::ERR_INVALID);

			Lxip::ssize_t res;

			enum { DEFAULT_BACKLOG = 5 };
			unsigned long backlog = DEFAULT_BACKLOG;
			len = handle.write_callback(
				_content_buffer, len, Callback::PARTIAL);
			Genode::ascii_to_unsigned(_content_buffer, backlog, len);

			res = _sock.ops->listen(&_sock, backlog);
			if (res != 0) return handle.write_status(Callback::ERR_IO);

			_content_buffer[len+0] = '\n';
			_content_buffer[len+1] = '\0';

			_parent.listen(true);

			handle.write_status(Callback::COMPLETE);
		}

		void read(Lxip_vfs_handle &handle, file_size len) override
		{
			if (handle.seek() != 0)            return handle.read_status(Callback::ERR_IO);
			if (len < sizeof(_content_buffer)) return handle.read_status(Callback::ERR_IO);

			Genode::size_t const n = Genode::strlen(_content_buffer);
			handle.read_callback(_content_buffer, n, Callback::COMPLETE);
		}
};


class Vfs::Lxip_connect_file : public Vfs::Lxip_file
{
	private:

		bool _connecting   = false;
		bool _is_connected = false;

	public:

		Lxip_connect_file(Lxip::Socket_dir &p, struct Linux::socket &s)
		: Lxip_file(p, s, "connect") { }

		/********************
		 ** File interface **
		 ********************/

		void write(Lxip_vfs_handle &handle, file_size len) override
		{
			using namespace Linux;

			if (handle.seek() != 0)
				return handle.write_status(Callback::ERR_INVALID);
			if (len > (sizeof(_content_buffer) - 2))
				return handle.write_status(Callback::ERR_INVALID);

			handle.write_callback(_content_buffer, len, Callback::PARTIAL);
			_content_buffer[len] = '\0';

			struct sockaddr_storage addr_storage;
			sockaddr_in *addr = (sockaddr_in*)&addr_storage;
			long tmp = remove_port(_content_buffer);
			addr->sin_port = htons(tmp);
			addr->sin_addr.s_addr = get_addr(_content_buffer);
			addr->sin_family = AF_INET;

			int res = _sock.ops->connect(&_sock, (sockaddr*)addr, sizeof(addr_storage), 0);

			switch (res) {
			case Lxip::Io_result::LINUX_EINPROGRESS:
				_connecting = true;
				return handle.write_status(Callback::ERR_INVALID);

			case Lxip::Io_result::LINUX_EALREADY:
				return handle.write_status(Callback::ERR_INVALID);

			case Lxip::Io_result::LINUX_EISCONN:
				/*
				 * Connecting on an already connected socket is an error.
				 * If we get this error after we got EINPROGRESS it is
				 * fine.
				 */
				if (_is_connected || !_connecting) {
					return handle.write_status(Callback::ERR_INVALID);
				}
				_is_connected = true;
				break;
			default:
				if (res != 0) {
					Genode::error("LxIP failed to connect, error ", res);
					return handle.write_status(Callback::ERR_IO);
				}
				break;
			}

			sockaddr_in *to_addr = (sockaddr_in*)&_parent.to_addr();
			to_addr->sin_port = htons(remove_port(_content_buffer));
			to_addr->sin_addr.s_addr = get_addr(_content_buffer);
			to_addr->sin_family = AF_INET;
			_parent.connect(true);

			handle.write_status(Callback::COMPLETE);
		}
}

;
class Vfs::Lxip_local_file : public Vfs::Lxip_file
{
	public:

		Lxip_local_file(Lxip::Socket_dir &p, struct Linux::socket &s)
		: Lxip_file(p, s, "local") { }

		/********************
		 ** File interface **
		 ********************/

		void read(Lxip_vfs_handle &handle, file_size len) override
		{
			if (handle.seek() != 0)            return handle.read_status(Callback::ERR_IO);
			if (len < sizeof(_content_buffer)) return handle.read_status(Callback::ERR_IO);

			using namespace Linux;

			sockaddr_storage storage;
			sockaddr_in *addr = (sockaddr_in *)&storage;

			int tmp_len = sizeof(storage);
			int err = _sock.ops->getname(&_sock, (sockaddr *)addr, &tmp_len, 0);
			if (err)
				return handle.read_status(Callback::ERR_IO);

			in_addr const   i_addr = addr->sin_addr;
			unsigned char const *a = (unsigned char*)&i_addr.s_addr;
			unsigned char const *p = (unsigned char*)&addr->sin_port;
			file_size n = Genode::snprintf(
				_content_buffer, len, "%d.%d.%d.%d:%u",
				 a[0], a[1], a[2], a[3], (p[0]<<8)|(p[1]<<0));

			_content_buffer[n++] = '\n';
			handle.read_callback(_content_buffer, n, Callback::COMPLETE);
		}
};


class Vfs::Lxip_remote_file : public Vfs::Lxip_file
{
	public:

		Lxip_remote_file(Lxip::Socket_dir &p, struct Linux::socket &s)
		: Lxip_file(p, s, "remote") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override
		{
			using namespace Linux;

			struct file f;
			f.f_flags = 0;

			return _sock.ops->poll(&f, &_sock, nullptr) & (POLLIN);
		}

		void read(Lxip_vfs_handle &handle, file_size len) override
		{
			if (handle.seek() != 0)            return handle.read_status(Callback::ERR_IO);
			if (len < sizeof(_content_buffer)) return handle.read_status(Callback::ERR_IO);

			using namespace Linux;

			sockaddr_storage storage;

			switch (_parent.parent().type()) {
			case Lxip::Protocol_dir::TYPE_STREAM: {
			int tmp_len = sizeof(storage);
				if (int err = _sock.ops->getname(&_sock, (sockaddr *)&storage, &tmp_len, 1)) {
					Genode::error("LxIP: getname failed, error ",-err);
					return handle.read_status(Callback::ERR_IO);
				}
				break;
			}
			case Lxip::Protocol_dir::TYPE_DGRAM: {
				struct msghdr   msg;
				struct iovec    iov;

				msg.msg_name       = &storage;
				msg.msg_namelen    = sizeof(storage);
				msg.msg_iter.type  = 0;
				msg.msg_iter.iov_offset = 0;
				msg.msg_iter.iov   = &iov;
				msg.msg_iter.nr_segs = 1;
				msg.msg_control    = nullptr;
				msg.msg_controllen = 0;
				msg.msg_iocb       = nullptr;

				iov.iov_base = _content_buffer;
				iov.iov_len  = sizeof(_content_buffer);

				if (int err = _sock.ops->recvmsg(&_sock, &msg, 0, MSG_DONTWAIT|MSG_PEEK)) {
					Genode::error("LxIP: peek failed, error ",-err);
					return handle.read_status(Callback::COMPLETE);
				}
			} }

			sockaddr_in *addr = (sockaddr_in *)&storage;
			in_addr const   i_addr = addr->sin_addr;
			unsigned char const *a = (unsigned char*)&i_addr.s_addr;
			unsigned char const *p = (unsigned char*)&addr->sin_port;
			file_size n = Genode::snprintf(
				_content_buffer, len, "%d.%d.%d.%d:%u",
				 a[0], a[1], a[2], a[3], (p[0]<<8)|(p[1]<<0));

			_content_buffer[n++] = '\n';
			handle.read_callback(_content_buffer, n, Callback::COMPLETE);
		}

		void write(Lxip_vfs_handle &handle, file_size len) override
		{
			using namespace Linux;

			sockaddr_in *remote_addr = (sockaddr_in*)&_parent.to_addr();

			len = min(len, sizeof(_content_buffer)-1);
			handle.write_callback(_content_buffer, len, Callback::PARTIAL);
			_content_buffer[len] = '\0';

			remote_addr->sin_port = htons(remove_port(_content_buffer));
			remote_addr->sin_addr.s_addr = get_addr(_content_buffer);
			remote_addr->sin_family = AF_INET;

			handle.write_status(Callback::COMPLETE);
		}
};


class Vfs::Lxip_accept_file : public Vfs::Lxip_file
{
	public:

		Lxip_accept_file(Lxip::Socket_dir &p, struct Linux::socket &s)
		: Lxip_file(p, s, "accept") { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override
		{
			using namespace Linux;

			struct file f;
			f.f_flags = 0;

			return _sock.ops->poll(&f, &_sock, nullptr) & (POLLIN);
		}

		void read(Lxip_vfs_handle &handle, file_size len) override
		{
			using namespace Linux;

			if (len < Lxip::MAX_FD_STR_LEN)
				return handle.read_status(Callback::ERR_IO);

			struct socket *new_sock = sock_alloc();

			if (int err = _sock.ops->accept(&_sock, new_sock, O_NONBLOCK)) {
				kfree(new_sock);
				return handle.read_status((err == -EAGAIN) ?
					Callback::COMPLETE : Callback::ERR_IO);
			}

			new_sock->type = _sock.type;
			new_sock->ops  = _sock.ops;

			try {
				unsigned const id = _parent.accept(*new_sock);
				Genode::size_t n = Genode::snprintf(
					_content_buffer, sizeof(_content_buffer), "%s/%u", _parent.top_dir(), id);
				handle.read_callback(_content_buffer, n, Callback::COMPLETE);
			} catch (...) {
				Genode::error("LxIP could not adopt new client socket");
				handle.read_status(Callback::ERR_IO);
			}
		}
};


class Vfs::Lxip_socket_dir final : public Vfs::Directory,
                                   public Lxip::Socket_dir
{
	public:

		enum {
			ACCEPT_NODE, BIND_NODE, CONNECT_NODE, CONTROL_NODE,
			DATA_NODE, LOCAL_NODE, LISTEN_NODE, REMOTE_NODE,
			FROM_NODE, TO_NODE,
			MAX_NODES
		};

	private:

		Genode::Allocator  &_alloc;

		Lxip::Protocol_dir   &_parent;
		struct Linux::socket &_sock;

		Vfs::Node *_nodes[MAX_NODES];

		struct Linux::sockaddr_storage _to_addr;

		unsigned _num_nodes()
		{
			unsigned n = 0;
			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				n += (_nodes[i] != nullptr);
			return n;
		}

		Lxip_accept_file  _accept_file  { *this, _sock };
		Lxip_bind_file    _bind_file    { *this, _sock };
		Lxip_connect_file _connect_file { *this, _sock };
		Lxip_data_file    _data_file    { *this, _sock };
		Lxip_listen_file  _listen_file  { *this, _sock };
		Lxip_local_file   _local_file   { *this, _sock };
		Lxip_remote_file  _remote_file  { *this, _sock };

		char _name[Lxip::MAX_SOCKET_NAME_LEN];

	public:

		Lxip_socket_dir(Genode::Allocator &alloc, unsigned id,
		                Lxip::Protocol_dir &parent,
		                struct Linux::socket &sock,
		                bool from_accept)
		:
			Directory(_name), _alloc(alloc),
			_parent(parent), _sock(sock)
		{
			Genode::snprintf(_name, sizeof(_name), "%u", id);

			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				_nodes[i] = nullptr;

			_nodes[BIND_NODE]    = &_bind_file;
			_nodes[CONNECT_NODE] = &_connect_file;
			_nodes[DATA_NODE]    = &_data_file;
			_nodes[LOCAL_NODE]   = &_local_file;
			_nodes[REMOTE_NODE] = &_remote_file;
		}

		~Lxip_socket_dir()
		{
			struct Linux::socket *sock = &_sock;

			if (sock->ops)
				sock->ops->release(sock);

			kfree(sock->wq);
			kfree(sock);
		}


		/**************************
		 ** Socket_dir interface **
		 **************************/

		Lxip::Protocol_dir &parent() override { return _parent; }

		Linux::sockaddr_storage &to_addr() override { return _to_addr; }

		char const *top_dir() override { return _parent.top_dir(); }

		unsigned accept(struct Linux::socket &sock) {
			return _parent.adopt_socket(sock, true); }

		void bind(bool v)
		{
			_nodes[LISTEN_NODE] = v ? &_listen_file : nullptr;
		}

		long bind() { return _bind_file.port(); }

		bool lookup_port(long port) { return _parent.lookup_port(port); }

		void connect(bool v)
		{
			_nodes[REMOTE_NODE] = /*v ?*/ &_remote_file/* : nullptr*/;
		}

		void listen(bool v)
		{
			_nodes[ACCEPT_NODE] = v ? &_accept_file : nullptr;
		}


		/*************************
		 ** Directory interface **
		 *************************/

		Vfs::Node *child(char const *name)
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++) {
				if (!_nodes[i]) continue;

				if (Genode::strcmp(_nodes[i]->name(), name) == 0) {
					return _nodes[i];
				}
			}

			return nullptr;
		}

		void dirent(file_offset index, Directory_service::Dirent &out)
		{
			out.fileno  = index+1;
			out.type    = Directory_service::DIRENT_TYPE_END;
			out.name[0] = '\0';

			if ((unsigned long long)index > MAX_NODES) return;

			if (!_nodes[index]) return;

			strncpy(out.name, _nodes[index]->name(), sizeof(out.name));

			Vfs::Lxip_file *file = dynamic_cast<Vfs::Lxip_file*>(_nodes[index]);
			if (file) {
				out.type = Directory_service::DIRENT_TYPE_FILE;
			}
		}

		file_size num_dirent() { return _num_nodes(); }

		bool _closed = false;

		void close() override
		{
			_closed = true;
		}

		bool closed() override
		{
			return _closed;
		}
};


class Vfs::Lxip_new_socket_file : public Vfs::File
{
	private:

		Lxip::Protocol_dir &_parent;

	public:

		Lxip_new_socket_file(Lxip::Protocol_dir &parent)
		: Vfs::File("new_socket"), _parent(parent) { }

		void read(Lxip_vfs_handle &handle, file_size len) override
		{
			using namespace Linux;

			if (handle.seek() != 0)
				return handle.read_status(Callback::ERR_INVALID);

			if (len < Lxip::MAX_FD_STR_LEN) {
				Genode::error("LxIP could not read new_socket file, buffer too small");
				return handle.read_status(Callback::ERR_INVALID);
			}

			struct Linux::socket *sock = nullptr;

			int type = (_parent.type() == Lxip::Protocol_dir::TYPE_STREAM)
				 ? SOCK_STREAM : SOCK_DGRAM;

			if (sock_create_kern(nullptr, AF_INET, type, 0, &sock)) {
				kfree(sock);
				return handle.read_status(Callback::ERR_IO);
			}

			try {
				char tmp[Lxip::MAX_FD_STR_LEN];
				unsigned const id = _parent.adopt_socket(*sock, false);
				file_size n = Genode::snprintf(tmp, len, "%s/%u", _parent.top_dir(), id);

				handle.read_callback(tmp, n, Callback::COMPLETE);
			} catch (...) {
				Genode::error("LxIP could not adopt socket");
				handle.read_status(Callback::ERR_IO);
			}
		}
};


class Lxip::Protocol_dir_impl : public Protocol_dir,
                                public Vfs::Directory
{
	private:

		Genode::Allocator &_alloc;

		/**
		 * Interface to LXIP stack ?
		 */

		Vfs::File_system &_parent;

		Type const _type;


		/**************************
		 ** Simple node registry **
		 **************************/

		enum { MAX_NODES = Lxip::MAX_SOCKETS + 1 };
		Vfs::Node *_nodes[MAX_NODES];

		unsigned _num_nodes()
		{
			unsigned n = 0;
			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				n += (_nodes[i] != nullptr);
			return n;
		}

		Vfs::Node **_unused_node()
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				if (_nodes[i] == nullptr) return &_nodes[i];
			throw -1;
		}

		void _free_node(Vfs::Node *node)
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				if (_nodes[i] == node) {
					_nodes[i] = nullptr;
					break;
				}
		}

		bool _is_root(const char *path)
		{
			return (Genode::strcmp(path, "") == 0) || (Genode::strcmp(path, "/") == 0);
		}

		Vfs::Lxip_new_socket_file _new_socket_file { *this };

	public:

		Protocol_dir_impl(Genode::Allocator       &alloc,
		                  Vfs::File_system         &parent,
		                  char               const *name,
		                  Lxip::Protocol_dir::Type  type)
		: Directory(name), _alloc(alloc), _parent(parent), _type(type)
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++) {
				_nodes[i] = nullptr;
			}

			_nodes[0] = &_new_socket_file;
		}

		~Protocol_dir_impl() { }

		Vfs::Node *lookup(char const *path)
		{
			if (*path == '/') path++;
			if (*path == '\0') return this;

			char const *p = path;
			while (*++p && *p != '/');

			for (Genode::size_t i = 0; i < MAX_NODES; i++) {
				if (!_nodes[i]) continue;

				if (Genode::strcmp(_nodes[i]->name(), path, (p - path)) == 0) {
					Vfs::Directory *dir = dynamic_cast<Directory *>(_nodes[i]);
					if (!dir) return _nodes[i];

					Socket_dir *socket = dynamic_cast<Socket_dir *>(_nodes[i]);
					if (socket && socket->closed())
						return nullptr;

					if (*p == '/') return dir->child(p+1);
					else           return dir;
				}
			}

			return nullptr;
		}


		/****************************
		 ** Protocol_dir interface **
		 ****************************/

		char const *top_dir() override { return name(); }

		Type type() { return _type; }

		unsigned adopt_socket(struct Linux::socket &sock, bool from_accept)
		{
			Vfs::Node **node = _unused_node();
			if (!node) throw -1;

			unsigned const id = ((unsigned char*)node - (unsigned char*)_nodes)/sizeof(*_nodes);

			Vfs::Lxip_socket_dir *dir = new (&_alloc)
				Vfs::Lxip_socket_dir(_alloc, id, *this, sock, from_accept);

			*node = dir;
			return id;
		}

		bool lookup_port(long port)
		{
			for (Genode::size_t i = 0; i < MAX_NODES; i++) {
				if (_nodes[i] == nullptr) continue;

				Lxip::Socket_dir *dir = dynamic_cast<Lxip::Socket_dir*>(_nodes[i]);
				if (dir && dir->bind() == port) return true;
			}
			return false;
		}


		/*************************
		 ** Directory interface **
		 *************************/

		void dirent(Vfs::file_offset index, Vfs::Directory_service::Dirent &out)
		{
			out.fileno  = index+1;
			out.type    = Vfs::Directory_service::DIRENT_TYPE_END;
			out.name[0] = '\0';

			if ((unsigned long long)index > MAX_NODES) return;

			if (!_nodes[index]) return;

			Genode::strncpy(out.name, _nodes[index]->name(), sizeof(out.name));

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(_nodes[index]);
			if (dir) {
				out.type = Vfs::Directory_service::DIRENT_TYPE_DIRECTORY;
			}

			Vfs::Lxip_file *file = dynamic_cast<Vfs::Lxip_file*>(_nodes[index]);
			if (file) {
				out.type = Vfs::Directory_service::DIRENT_TYPE_FILE;
			}
		}

		Vfs::Node *child(char const *name) { return nullptr; }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Vfs::Directory_service::Stat_result
		stat(char const *path, Vfs::Directory_service::Stat &out)
		{
			out = Vfs::Directory_service::Stat();

			Vfs::Node *node = lookup(path);
			if (!node) return Vfs::Directory_service::STAT_ERR_NO_ENTRY;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (dir) {
				out.mode = Vfs::Directory_service::STAT_MODE_DIRECTORY | 0777;
				return Vfs::Directory_service::STAT_OK;
			}

			Vfs::File *file = dynamic_cast<Vfs::File*>(node);
			if (file) {
				out.mode = Vfs::Directory_service::STAT_MODE_FILE | 0666;
				out.size = 0x1000;  /* there may be something to read */
				return Vfs::Directory_service::STAT_OK;
			}

			return Vfs::Directory_service::STAT_ERR_NO_ENTRY;
		}

		Vfs::Directory_service::Unlink_result
		unlink(char const *path)
		{
			Vfs::Node *node = lookup(path);
			if (!node) return Vfs::Directory_service::UNLINK_ERR_NO_ENTRY;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (!dir) return Vfs::Directory_service::UNLINK_ERR_NO_ENTRY;

			_free_node(node);

			Genode::destroy(&_alloc, dir);

			return Vfs::Directory_service::UNLINK_OK;
		}
};


/*******************************
 ** Filesystem implementation **
 *******************************/

class Vfs::Lxip_file_system : public Vfs::File_system,
                              public Vfs::Directory
{
	private:

		Genode::Entrypoint &_ep;
		Genode::Allocator  &_alloc;

		Lxip::Protocol_dir_impl _tcp_dir {
			_alloc, *this, "tcp", Lxip::Protocol_dir::TYPE_STREAM };
		Lxip::Protocol_dir_impl _udp_dir {
			_alloc, *this, "udp", Lxip::Protocol_dir::TYPE_DGRAM  };

		Vfs::Node *_lookup(char const *path)
		{
			if (*path == '/') path++;
			if (*path == '\0') return this;

			if (Genode::strcmp(path, "tcp", 3) == 0)
				return _tcp_dir.lookup(&path[3]);
			if (Genode::strcmp(path, "udp", 3) == 0)
				return _udp_dir.lookup(&path[3]);
			return nullptr;
		}

		bool _is_root(const char *path)
		{
			return (strcmp(path, "") == 0) || (strcmp(path, "/") == 0);
		}

	public:

		Lxip_file_system(Genode::Env &env, Genode::Allocator &alloc)
		: Directory(""), _ep(env.ep()), _alloc(alloc)
		{ }

		~Lxip_file_system() { }

		char const *name() { return "lxip"; }


		/*************************
		 ** Directory interface **
		 *************************/

		void dirent(file_offset index, Directory_service::Dirent &out)
		{
			if (index == 0) {
				out.fileno  = (Genode::addr_t)&_tcp_dir;
				out.type    = Directory_service::DIRENT_TYPE_DIRECTORY;
				Genode::strncpy(out.name, "tcp", sizeof(out.name));
			} else if (index == 1) {
				out.fileno  = (Genode::addr_t)&_udp_dir;
				out.type    = Directory_service::DIRENT_TYPE_DIRECTORY;
				Genode::strncpy(out.name, "udp", sizeof(out.name));
			} else {
				out.fileno  = 0;
				out.type    = Directory_service::DIRENT_TYPE_END;
				out.name[0] = '\0';
			}
		}

		Vfs::Node *child(char const *name) { return nullptr; }


		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *path) override {
			return Dataspace_capability(); }

		void release(char const *path, Dataspace_capability ds_cap) override { }

		Stat_result stat(char const *path, Stat &out) override
		{
			Vfs::Node *node = _lookup(path);
			if (!node) return STAT_ERR_NO_ENTRY;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (dir) {
				out.mode = STAT_MODE_DIRECTORY | 0777;
				return STAT_OK;
			}

			Vfs::File *file = dynamic_cast<Vfs::File*>(node);
			if (file) {
				out.mode = STAT_MODE_FILE | 0666;
				out.size = 0x1000;  /* there may be something to read */
				return STAT_OK;
			}

			return STAT_ERR_NO_ENTRY;
		}

		Dirent_result dirent(char const *path, file_offset index, Dirent &out) override
		{
			Vfs::Node *node = _lookup(path);
			if (!node) return DIRENT_ERR_INVALID_PATH;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (dir) {
				dir->dirent(index, out);
				return DIRENT_OK;
			}

			return DIRENT_ERR_INVALID_PATH;
		}

		file_size num_dirent(char const *path) override
		{
			if (_is_root(path)) return 2;

			Vfs::Node *node = _lookup(path);
			if (!node) return 0;

			Vfs::Directory *dir = dynamic_cast<Vfs::Directory*>(node);
			if (!dir) return 0;

			return dir->num_dirent();
		}

		bool directory(char const *path) override
		{
			Vfs::Node *node = _lookup(path);
			return node ? dynamic_cast<Vfs::Directory *>(node) : 0;
		}

		char const *leaf_path(char const *path) override
		{
			return path;
		}

		Open_result open(char const *path, unsigned mode,
		                 Vfs_handle **out_handle,
		                 Genode::Allocator &alloc) override
		{
			if (mode & OPEN_MODE_CREATE) return OPEN_ERR_NO_PERM;

			Vfs::Node *node = _lookup(path);
			if (!node) return OPEN_ERR_UNACCESSIBLE;

			Vfs::File *file = dynamic_cast<Vfs::File*>(node);
			if (file) {
				Lxip_vfs_handle *handle =
					new (alloc) Vfs::Lxip_vfs_handle(*this, alloc, 0, *file);
				*out_handle = handle;

				if (dynamic_cast<Lxip_file*>(file))
					_polling_handles.insert(handle);

				return OPEN_OK;
			}

			return OPEN_ERR_UNACCESSIBLE;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);
			if (handle) {
				_polling_handles.remove(handle);
				Genode::destroy(handle->alloc(), handle);
			}
		}

		Unlink_result unlink(char const *path) override
		{
			if (*path == '/') path++;

			if (Genode::strcmp(path, "tcp", 3) == 0)
				return _tcp_dir.unlink(&path[3]);
			if (Genode::strcmp(path, "udp", 3) == 0)
				return _udp_dir.unlink(&path[3]);
			return UNLINK_ERR_NO_ENTRY;
		}

		Readlink_result readlink(char const *, char *, file_size,
		                         file_size &) override {
			return READLINK_ERR_NO_ENTRY; }

		Rename_result rename(char const *, char const *) override {
			return RENAME_ERR_NO_PERM; }

		Mkdir_result mkdir(char const *, unsigned) override {
			return MKDIR_ERR_NO_PERM; }

		Symlink_result symlink(char const *, char const *) override {
			return SYMLINK_ERR_NO_ENTRY; }


		/*************************************
		 ** Lxip_file I/O service interface **
		 *************************************/

		void write(Vfs_handle *vfs_handle, file_size len) override
		{
			Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);

			handle->file.write(*handle, len);
		}

		void read(Vfs_handle *vfs_handle, file_size len) override
		{
			Lxip_vfs_handle *handle =
				static_cast<Lxip_vfs_handle*>(vfs_handle);

			handle->file.read(*handle, len);
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			/* report ok because libc always executes ftruncate() when opening rw */
			return FTRUNCATE_OK;
		}

		bool subscribe(Vfs::Vfs_handle *) override { return true; }
};


struct Lxip_factory : Vfs::File_system_factory
{
	struct Init
	{
		char _config_buf[128];

		char *_parse_config(Genode::Xml_node);

		Init(Genode::Env       &env,
		     Genode::Allocator &alloc,
		     Genode::Xml_node  config)
		{
			Lx::timer_init(env, alloc, &poll_all);
			Lx::event_init(env, &poll_all);
			Lx::nic_client_init(env, alloc, &poll_all);

			lxip_init(_parse_config(config));
		}
	};

	Vfs::File_system *create(Genode::Env       &env,
	                         Genode::Allocator &alloc,
	                         Genode::Xml_node  config) override
	{
		static Init inst(env, alloc, config);
		return new (alloc) Vfs::Lxip_file_system(env, alloc);
	}
};


char *Lxip_factory::Init::_parse_config(Genode::Xml_node config)
{
	using namespace Genode;

	typedef String<16> Addr;

	try {

		Addr ip_addr = config.attribute_value("ip_addr", Addr());
		Addr netmask = config.attribute_value("netmask", Addr());
		Addr gateway = config.attribute_value("gateway", Addr());

		/* either none or all 3 interface attributes must exist */
		if (ip_addr == "") {
			error("Missing \"ip_addr\" attribute. Ignoring network interface config.");
			throw Genode::Xml_node::Nonexistent_attribute();
		} else if (netmask == "") {
			error("Missing \"netmask\" attribute. Ignoring network interface config.");
			throw Genode::Xml_node::Nonexistent_attribute();
		} else if (gateway == "") {
			error("Missing \"gateway\" attribute. Ignoring network interface config.");
			throw Genode::Xml_node::Nonexistent_attribute();
		}

		log("static network interface: ip_addr=",ip_addr," netmask=",netmask," gateway=",gateway);

		Genode::snprintf(_config_buf, sizeof(_config_buf), "%s::%s:%s:::off",
		                 ip_addr.string(), gateway.string(), netmask.string());
	} catch (...) {
		log("Using DHCP for interface configuration.");
		Genode::strncpy(_config_buf, "dhcp", sizeof(_config_buf));
	}

	log("init_libc_lxip() address config=", (char const *)_config_buf);

	return _config_buf;
}


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Lxip_factory factory;
	return &factory;
}
