/*
 * \brief  lxip-based socket file system
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
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

/* Lxip includes */
#include <lxip/lxip.h>

/* Libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


namespace {

	char const *get_port(char const *s)
	{
		char const *p = s;
		while (*++p) {
			if (*p == ':')
				return ++p;
		}
		return nullptr;
	}

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

}


namespace Lxip {

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
	virtual char const *top_dir() = 0;
	virtual Lxip::Type type() = 0;
	virtual unsigned adopt_socket(Lxip::Handle &, bool) = 0;
	virtual bool     lookup_port(long) = 0;
};


struct Lxip::Socket_dir
{
	virtual char const *top_dir() = 0;
	virtual unsigned accept(Lxip::Handle &) = 0;
	virtual void     bind(bool) = 0; /* bind to port */
	virtual long     bind() = 0; /* return bound port */
	virtual bool     lookup_port(long) = 0;
	virtual void     connect(bool) = 0;
	virtual void     listen(bool) = 0;
	virtual sockaddr_storage &to_addr() = 0;
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
	class Lxip_from_file;
	class Lxip_to_file;
	class Lxip_socket_dir;

	class Lxip_new_socket_file;

	class Lxip_vfs_handle;
	class Lxip_file_system;

	typedef Genode::List<Lxip_vfs_handle> Lxip_vfs_handles;

	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Read_result   Read_result;
}


/***************
 ** Vfs nodes **
 ***************/

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
	virtual Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out)
	{
		return Read_result::READ_ERR_INVALID;
	}

	/**
	 * Pass len data to handle write callback
	 */
	virtual Write_result write(Lxip_vfs_handle &handle, file_size len, file_size &out)
	{
		return Write_result::WRITE_ERR_INVALID;
	}

	/**
	 * Check for data to read or write
	 */
	virtual bool poll() { return false; }
};


struct Vfs::Lxip_vfs_handle final : Vfs::Vfs_handle, Lxip_vfs_handles::Element
{
	Vfs::File &file;

	Lxip_vfs_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                Vfs::File &file)
	: Vfs_handle(fs, fs, alloc, status_flags), file(file) { }
};


/**
 * List of open handles to potentially poll
 *
 * Could be a dynamic queue, but this works for now.
 */
static Vfs::Lxip_vfs_handles _polling_handles;

static void poll_all()
{
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
		Lxip::Socketcall &_sc;
		Lxip::Handle     &_handle;

		char _content_buffer[Lxip::MAX_DATA_LEN];

	public:

		Lxip_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h,
			 char const *name)
		: Vfs::File(name), _parent(p), _sc(s), _handle(h) { }

		virtual ~Lxip_file() { }

		/********************
		 ** File interface **
		 ********************/

		bool poll() override {
			return _sc.poll(_handle, false); };
};


class Vfs::Lxip_data_file : public Vfs::Lxip_file
{
	public:

		Lxip_data_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "data") { }

		/********************
		 ** File interface **
		 ********************/

		Write_result write(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			file_size remain = len;
			while (remain) {
				file_size n = min(remain, sizeof(_content_buffer));
				n = handle.write_callback(_content_buffer, n, Callback::PARTIAL);

				Lxip::ssize_t res = _sc.send(_handle, _content_buffer, n,
				                             0 /* flags */,
				                             0 /* familiy */,
				                             nullptr /* addr */);

				if ((res == Lxip::ssize_t(Lxip::Io_result::LINUX_EAGAIN))
				 || (file_size(res) < n)) {
					Genode::error("short write to lxIP socket, returning error");
					handle.write_callback(_content_buffer, 0, Callback::ERROR);
					return Write_result::WRITE_ERR_IO;
				} else if (res < 0) {
					Genode::error("send error ", res);
					handle.write_callback(_content_buffer, 0, Callback::ERROR);
					return Write_result::WRITE_ERR_IO;
				}

				out += res;
				remain -= res;
			}

			handle.write_callback(_content_buffer, 0, Callback::COMPLETE);
			return Write_result::WRITE_OK;
		}


		Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			file_size remain = len;
			while (remain) {
				Lxip::ssize_t const n = min(remain, sizeof(_content_buffer));
				Lxip::ssize_t const res = _sc.recv(_handle, _content_buffer, n,
				                                   0 /* flags */,
				                                   0 /* familiy */,
				                                   nullptr /* addr */,
				                                   nullptr /* len */);
				if (res < 0) {
					if (res == Lxip::Io_result::LINUX_EAGAIN) {
						out = 0;
						return Read_result::READ_OK;
					} else {
						Genode::error("recv error ", res);
						return Read_result::READ_ERR_IO;
					}
				} else if (res < n) {
					/* short read means success */
					handle.read_callback(_content_buffer, res, Callback::COMPLETE);
					out += res;
					return Read_result::READ_OK;
				}
				remain -= res;
				out += res;

				/* notify the callback if the operation is complete */
				Callback::Status s = remain ?
					Callback::COMPLETE : Callback::PARTIAL;
				handle.read_callback(_content_buffer, res, s);
			}
			return Read_result::READ_OK;
		}
};


class Vfs::Lxip_bind_file : public Vfs::Lxip_file
{
	private:

		long _port = -1;

	public:

		Lxip_bind_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "bind") { }

		long port() { return _port; }


		/********************
		 ** File interface **
		 ********************/

		Write_result write(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (handle.seek() != 0)
				return Write_result::WRITE_ERR_INVALID;
			if (len > (sizeof(_content_buffer) - 2))
				return Write_result::WRITE_ERR_INVALID;

			/* already bound to port */
			if (_port >= 0) return Write_result::WRITE_ERR_INVALID;

			/* check if port is already used by other socket */
			long tmp = -1;
			len = handle.write_callback(
				_content_buffer, len, Callback::COMPLETE);
			Genode::ascii_to_unsigned(get_port(_content_buffer), tmp, len);
			if (tmp == -1)
				return Write_result::WRITE_ERR_INVALID;
			if (_parent.lookup_port(tmp))
				return Write_result::WRITE_ERR_INVALID;

			/* port is free, try to bind it */
			Lxip::ssize_t res = _sc.bind_port(_handle, _content_buffer);
			if (res != 0) return Write_result::WRITE_ERR_IO;

			_port = tmp;

			_content_buffer[len+0] = '\n';
			_content_buffer[len+1] = '\0';
			out = len;

			_parent.bind(true);
			return Write_result::WRITE_OK;
		}

		Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (handle.seek() != 0)            return Read_result::READ_ERR_IO;
			if (len < sizeof(_content_buffer)) return Read_result::READ_ERR_IO;

			Genode::size_t const n = Genode::strlen(_content_buffer);
			out = n;
			handle.read_callback(_content_buffer, n, Callback::COMPLETE);
			return Read_result::READ_OK;
		}
};


class Vfs::Lxip_listen_file : public Vfs::Lxip_file
{
	public:

		Lxip_listen_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "listen") { }

		/********************
		 ** File interface **
		 ********************/

		Write_result write(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (handle.seek() != 0)
				return Write_result::WRITE_ERR_INVALID;
			if (len > (sizeof(_content_buffer) - 2))
				return Write_result::WRITE_ERR_INVALID;

			Lxip::ssize_t res;

			enum { DEFAULT_BACKLOG = 5 };
			unsigned long result = DEFAULT_BACKLOG;
			len = handle.write_callback(
				_content_buffer, len, Callback::COMPLETE);
			Genode::ascii_to_unsigned(_content_buffer, result, len);

			res = _sc.listen(_handle, result);
			if (res != 0) return Write_result::WRITE_ERR_IO;

			_content_buffer[len+0] = '\n';
			_content_buffer[len+1] = '\0';
			out = len;

			_parent.listen(true);

			return Write_result::WRITE_OK;
		}

		Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (handle.seek() != 0)            return Read_result::READ_ERR_IO;
			if (len < sizeof(_content_buffer)) return Read_result::READ_ERR_IO;

			Genode::size_t const n = Genode::strlen(_content_buffer);
			out = n;
			handle.read_callback(_content_buffer, n, Callback::COMPLETE);

			return Read_result::READ_OK;
		}
};


class Vfs::Lxip_connect_file : public Vfs::Lxip_file
{
	private:

		bool _connecting   = false;
		bool _is_connected = false;

	public:

		Lxip_connect_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "connect") { }

		/********************
		 ** File interface **
		 ********************/

		Write_result write(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (handle.seek() != 0)
				return Write_result::WRITE_ERR_INVALID;
			if (len > (sizeof(_content_buffer) - 2))
				return Write_result::WRITE_ERR_INVALID;

			handle.write_callback(_content_buffer, len, Callback::COMPLETE);

			int res = _sc.dial(_handle, _content_buffer);

			switch (res) {
			case Lxip::Io_result::LINUX_EINPROGRESS:
				_connecting = true;
				return Write_result::WRITE_ERR_WOULD_BLOCK;

			case Lxip::Io_result::LINUX_EALREADY:
				if (!_connecting) return Write_result::WRITE_ERR_INVALID;
				return Write_result::WRITE_ERR_WOULD_BLOCK;

			case Lxip::Io_result::LINUX_EISCONN:
				/*
				 * Connecting on an already connected socket is an error.
				 * If we get this error after we got EINPROGRESS it is
				 * fine.
				 */
				if (_is_connected || !_connecting)
					return Write_result::WRITE_ERR_INVALID;
				_is_connected = true;
				break;
			default:
				if (res != 0) return Write_result::WRITE_ERR_IO;
				break;
			}

			handle.write_callback(_content_buffer, len, Callback::COMPLETE);
			_content_buffer[len+0] = '\n';
			_content_buffer[len+1] = '\0';
			out = len;

			_parent.connect(true);

			return Write_result::WRITE_OK;
		}
}

;
class Vfs::Lxip_local_file : public Vfs::Lxip_file
{
	public:

		Lxip_local_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "local") { }

		/********************
		 ** File interface **
		 ********************/

		Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (handle.seek() != 0)            return Read_result::READ_ERR_IO;
			if (len < sizeof(_content_buffer)) return Read_result::READ_ERR_IO;

			int res = _sc.local(_handle, _content_buffer, sizeof(_content_buffer)-1);
			if (res < 0) return Read_result::READ_ERR_IO;
			Genode::size_t n = Genode::strlen(_content_buffer);
			_content_buffer[n++] = '\n';
			out = n;
			handle.read_callback(_content_buffer, n, Callback::COMPLETE);

			return Read_result::READ_OK;
		}
};


class Vfs::Lxip_remote_file : public Vfs::Lxip_file
{
	public:

		Lxip_remote_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "remote") { }

		/********************
		 ** File interface **
		 ********************/

		Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (handle.seek() != 0)            return Read_result::READ_ERR_IO;
			if (len < sizeof(_content_buffer)) return Read_result::READ_ERR_IO;

			int res = _sc.remote(_handle, _content_buffer, sizeof(_content_buffer)-1);
			if (res < 0) return Read_result::READ_ERR_IO;
			Genode::size_t n = strlen(_content_buffer);
			_content_buffer[n++] = '\n';
			out = n;
			handle.read_callback(_content_buffer, n, Callback::COMPLETE);

			return Read_result::READ_OK;
		}
};


class Vfs::Lxip_accept_file : public Vfs::Lxip_file
{
	public:

		Lxip_accept_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "accept") { }

		/********************
		 ** File interface **
		 ********************/

		Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (len < Lxip::MAX_FD_STR_LEN)
				return Read_result::READ_ERR_IO;

			Lxip::Handle h = _sc.accept(_handle, nullptr /* addr */, nullptr /* len */);

			if (!h.socket)
				return Read_result::READ_ERR_IO;

			/* check for EAGAIN */
			if (h.socket == (void*)0x1) {
				out = 0;
				return Read_result::READ_OK;
			}

			try {
				unsigned const id = _parent.accept(h);
				Genode::size_t n = Genode::snprintf(
					_content_buffer, sizeof(_content_buffer), "%s/%u", _parent.top_dir(), id);
				out = n;
				handle.read_callback(_content_buffer, n, Callback::COMPLETE);
				return Read_result::READ_OK;
			} catch (...) { Genode::error("Could not adopt new client socket"); }

			return Read_result::READ_ERR_IO;
		}
};


class Vfs::Lxip_from_file : public Vfs::Lxip_file
{
	public:

		Lxip_from_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "from") { }

		/********************
		 ** File interface **
		 ********************/

		Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			/* TODO: IPv6 */
			struct sockaddr_storage addr;
			Lxip::uint32_t addr_len = sizeof(addr);

			/* peek to get the address of the currently queued data */
			Lxip::ssize_t res = _sc.recv(_handle, _content_buffer, 0, MSG_PEEK,
			                             0 /* familiy */, &addr, &addr_len);
			if (res == Lxip::Io_result::LINUX_EAGAIN) {
				out = 0;
				return Read_result::READ_OK;
			}

			in_addr const   i_addr = ((struct sockaddr_in*)&addr)->sin_addr;
			if (i_addr.s_addr == 0) {
				out = 0;
				return Read_result::READ_OK;
			}
			unsigned char const *a = (unsigned char*)&i_addr.s_addr;
			unsigned char const *p = (unsigned char*)&((struct sockaddr_in*)&addr)->sin_port;

			/* TODO: big endian support */
			len = Genode::snprintf(_content_buffer, min(len, sizeof(_content_buffer)),
			                       "%d.%d.%d.%d:%u\n",
			                       a[0], a[1], a[2], a[3],
			                       (p[0]<<8)|(p[1]<<0));
			out = handle.read_callback(_content_buffer, len, Callback::COMPLETE);
			return Read_result::READ_OK;
		}
};


class Vfs::Lxip_to_file : public Vfs::Lxip_file
{
	public:

		Lxip_to_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "to") { }

		/********************
		 ** File interface **
		 ********************/

		Write_result write(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			len = min(len, sizeof(_content_buffer)-1);
			out = handle.write_callback(_content_buffer, len, Callback::COMPLETE);
			_content_buffer[len] = '\0';

			sockaddr_in *to_addr = (sockaddr_in*)&_parent.to_addr();

			to_addr->sin_port = htons(remove_port(_content_buffer));
			inet_pton(AF_INET, _content_buffer, &(to_addr->sin_addr));

			return Write_result::WRITE_OK;
		}
};

class Vfs::Lxip_socket_dir : public Vfs::Directory,
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

		Lxip::Protocol_dir &_parent;
		Lxip::Socketcall   &_sc;
		Lxip::Handle        _handle;

		Vfs::Node *_nodes[MAX_NODES];

		struct sockaddr_storage _to_addr;

		unsigned _num_nodes()
		{
			unsigned n = 0;
			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				n += (_nodes[i] != nullptr);
			return n;
		}

		Lxip_accept_file  _accept_file  { *this, _sc, _handle };
		Lxip_bind_file    _bind_file    { *this, _sc, _handle };
		Lxip_connect_file _connect_file { *this, _sc, _handle };
		Lxip_data_file    _data_file    { *this, _sc, _handle };
		Lxip_listen_file  _listen_file  { *this, _sc, _handle };
		Lxip_local_file   _local_file   { *this, _sc, _handle };
		Lxip_remote_file  _remote_file  { *this, _sc, _handle };
		Lxip_from_file    _from_file    { *this, _sc, _handle };
		Lxip_to_file      _to_file      { *this, _sc, _handle };

		char _name[Lxip::MAX_SOCKET_NAME_LEN];

	public:

		Lxip_socket_dir(Genode::Allocator &alloc, unsigned id,
		                Lxip::Protocol_dir &parent,
		                Lxip::Socketcall &sc, Lxip::Handle handle,
		                bool from_accept)
		:
			Directory(_name), _alloc(alloc),
			_parent(parent), _sc(sc), _handle(handle)
		{
			Genode::snprintf(_name, sizeof(_name), "%u", id);

			for (Genode::size_t i = 0; i < MAX_NODES; i++)
				_nodes[i] = nullptr;

			_nodes[BIND_NODE]    = &_bind_file;
			_nodes[CONNECT_NODE] = &_connect_file;
			_nodes[DATA_NODE]    = &_data_file;
			_nodes[FROM_NODE]    = &_from_file;
			_nodes[TO_NODE]      = &_to_file;

			if (from_accept) {
				_nodes[LOCAL_NODE]  = &_local_file;
				_nodes[REMOTE_NODE] = &_remote_file;
			}
		}

		~Lxip_socket_dir() { _sc.close(_handle); }


		/**************************
		 ** Socket_dir interface **
		 **************************/

		sockaddr_storage &to_addr() override { return _to_addr; }

		char const *top_dir() override { return _parent.top_dir(); }

		unsigned accept(Lxip::Handle &h) {
			return _parent.adopt_socket(h, true); }

		void bind(bool v)
		{
			_nodes[LISTEN_NODE] = v ? &_listen_file : nullptr;
		}

		long bind() { return _bind_file.port(); }

		bool lookup_port(long port) { return _parent.lookup_port(port); }

		void connect(bool v)
		{
			_nodes[REMOTE_NODE] = v ? &_remote_file : nullptr;
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
};


class Vfs::Lxip_new_socket_file : public Vfs::File
{
	private:

		Lxip::Protocol_dir &_parent;
		Lxip::Socketcall   &_sc;

	public:

		Lxip_new_socket_file(Lxip::Protocol_dir &parent, Lxip::Socketcall &sc)
		: Vfs::File("new_socket"), _parent(parent), _sc(sc) { }

		Read_result read(Lxip_vfs_handle &handle, file_size len, file_size &out) override
		{
			if (handle.seek() != 0)
				return Read_result::READ_ERR_INVALID;

			if (len < Lxip::MAX_FD_STR_LEN) {
				Genode::error("Could not read new_socket file, buffer too small");
				return Read_result::READ_ERR_INVALID;
			}

			Lxip::Handle h = _sc.socket(Lxip::TYPE_STREAM);
			if (!h.socket)
				return Read_result::READ_ERR_INVALID;

			h.non_block = true;

			try {
				char tmp[Lxip::MAX_FD_STR_LEN];
				unsigned const id = _parent.adopt_socket(h, false);
				file_size n = Genode::snprintf(tmp, len, "%s/%u", _parent.top_dir(), id);
				out = n;

				handle.read_callback(tmp, n, Callback::COMPLETE);
				return Read_result::READ_OK;
			} catch (...) {
				Genode::error("Could not adopt socket");
			}

			_sc.close(h);
			return Read_result::READ_ERR_IO;
		}
};


class Lxip::Protocol_dir_impl : public Protocol_dir,
                                public Vfs::Directory
{
	private:

		Genode::Allocator &_alloc;

		/**
		 * Interface to LXIP stack
		 */
		struct Lxip::Socketcall &_socketcall;

		Vfs::File_system &_parent;

		Lxip::Type const _type;


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

		Vfs::Lxip_new_socket_file _new_socket_file { *this, _socketcall };

	public:

		Protocol_dir_impl(Genode::Allocator &alloc,
		                  Lxip::Socketcall  &socketcall,
		                  Vfs::File_system  &parent,
		                  char        const *name,
		                  Lxip::Type         type)
		: Directory(name), _alloc(alloc), _socketcall(socketcall), _parent(parent), _type(type)
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

		Lxip::Type type() { return _type; }

		unsigned adopt_socket(Lxip::Handle &h, bool from_accept)
		{
			Vfs::Node **node = _unused_node();
			if (!node) throw -1;

			unsigned const id = ((unsigned char*)node - (unsigned char*)_nodes)/sizeof(*_nodes);

			Vfs::Lxip_socket_dir *dir = new (&_alloc)
				Vfs::Lxip_socket_dir(_alloc, id, *this, _socketcall, h, from_accept);

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

		Vfs::Directory_service::Open_result
		open(char const *path, unsigned mode,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator &alloc)
		{
			if (mode & Vfs::Directory_service::OPEN_MODE_CREATE)
				return Vfs::Directory_service::OPEN_ERR_NO_PERM;

			Vfs::Node *node = lookup(path);
			if (!node) return Vfs::Directory_service::OPEN_ERR_UNACCESSIBLE;

			Vfs::File *file = dynamic_cast<Vfs::File*>(node);
			if (file) {
				*out_handle = new (alloc) Vfs::Lxip_vfs_handle(_parent, alloc, 0, *file);
				return Vfs::Directory_service::OPEN_OK;
			}

			return Vfs::Directory_service::OPEN_ERR_UNACCESSIBLE;
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

		Genode::Xml_node _config;
		char             _config_buf[128];

		char *_parse_config(Genode::Xml_node);

		/**
		 * Interface to LXIP stack
		 */
		struct Lxip::Socketcall &_socketcall;

		Lxip::Protocol_dir_impl _tcp_dir { _alloc, _socketcall, *this, "tcp", Lxip::TYPE_STREAM };
		Lxip::Protocol_dir_impl _udp_dir { _alloc, _socketcall, *this, "udp", Lxip::TYPE_DGRAM  };

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

		Lxip_file_system(Genode::Env &env, Genode::Allocator &alloc, Genode::Xml_node config)
		:
			Directory("tcp"), _ep(env.ep()), _alloc(alloc), _config(config),
			_socketcall(Lxip::init(env, alloc, &poll_all, _parse_config(_config)))
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

		Write_result write(Vfs_handle *vfs_handle, file_size len, file_size &out) override
		{
			Lxip_vfs_handle *handle =
				static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle);

			return handle->file.write(*handle, len, out);
		}

		Read_result read(Vfs_handle *vfs_handle, file_size len, file_size &out) override
		{
			Lxip_vfs_handle *handle =
				static_cast<Lxip_vfs_handle*>(vfs_handle);

			return handle->file.read(*handle, len, out);
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			/* report ok because libc always executes ftruncate() when opening rw */
			return FTRUNCATE_OK;
		}

		void poll_io() override
		{
			/* XXX: a hack to block for progress */
			Genode::warning(__func__, " called, blocking entrypoint at lxip plugin");
			_ep.wait_and_dispatch_one_signal();
		}

		bool subscribe(Vfs::Vfs_handle *) override { return true; }
};


char *Vfs::Lxip_file_system::_parse_config(Genode::Xml_node config)
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


struct Lxip_factory : Vfs::File_system_factory
{
	Vfs::File_system *create(Genode::Env       &env,
	                         Genode::Allocator &alloc,
	                         Genode::Xml_node  config) override
	{
		return new (alloc) Vfs::Lxip_file_system(env, alloc, config);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Lxip_factory factory;
	return &factory;
}
