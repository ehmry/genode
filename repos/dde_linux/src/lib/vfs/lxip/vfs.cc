/*
 * \brief  lxip-based socket file system
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
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


Vfs::Ticker **vfs_ticker = nullptr;

static void vfs_tick()
{
	if (vfs_ticker && *vfs_ticker)
		(*vfs_ticker)->tick();
}

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

	struct Would_block { }; /* exception */

	virtual Lxip::ssize_t read(char *, Genode::size_t, file_size)        { return -1; }
	virtual Lxip::ssize_t write(char const *, Genode::size_t, file_size) { return -1; }
};


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
		: File(name), _parent(p), _sc(s), _handle(h) { }

		virtual ~Lxip_file() { }
};


class Vfs::Lxip_data_file : public Vfs::Lxip_file
{
	public:

		Lxip_data_file(Lxip::Socket_dir &p, Lxip::Socketcall &s, Lxip::Handle &h)
		: Lxip_file(p, s, h, "data") { }

		/********************
		 ** File interface **
		 ********************/

		Lxip::ssize_t write(char const *src, Genode::size_t len, file_size seek_offset)
		{
			sockaddr_in *addr = (sockaddr_in*)&_parent.to_addr();

			Lxip::ssize_t res = _sc.send(_handle, src, len,
			                             0 /* flags */, 0 /* family */,
			                             addr->sin_addr.s_addr ? addr : nullptr);
			if (res == Lxip::Io_result::LINUX_EAGAIN) throw Would_block();
			return res;
		}

		Lxip::ssize_t read(char *dst, Genode::size_t len, file_size seek_offset)
		{
			Lxip::ssize_t res = _sc.recv(_handle, dst, len,
			                             0 /* flags */, 0 /* familiy */, nullptr /* addr */,
			                             nullptr /* len */);
			if (res == Lxip::Io_result::LINUX_EAGAIN) throw Would_block();
			return res;
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

		Lxip::ssize_t write(char const *src, Genode::size_t len, file_size seek_offset) override
		{
			if (seek_offset != 0)                    return -1;
			if (len > (sizeof(_content_buffer) - 2)) return -1;

			/* already bound to port */
			if (_port >= 0) return -1;

			/* check if port is already used by other socket */
			long tmp = -1;
			Genode::ascii_to_unsigned(get_port(src), tmp, 10);
			if (tmp == -1)                return -1;
			if (_parent.lookup_port(tmp)) return -1;

			/* port is free, try to bind it */
			Lxip::ssize_t res = _sc.bind_port(_handle, src);
			if (res != 0) return -1;

			_port = tmp;

			Genode::memcpy(_content_buffer, src, len);
			_content_buffer[len+1] = '\n';
			_content_buffer[len+2] = '\0';

			_parent.bind(true);

			return len;
		}

		Lxip::ssize_t read(char *dst, Genode::size_t len, file_size seek_offset) override
		{
			if (seek_offset != 0)              return -1;
			if (len < sizeof(_content_buffer)) return -1;

			Genode::size_t const n = Genode::strlen(_content_buffer);
			Genode::memcpy(dst, _content_buffer, n);

			return n;
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

		Lxip::ssize_t write(char const *src, Genode::size_t len, file_size seek_offset) override
		{
			if (seek_offset != 0)                    return -1;
			if (len > (sizeof(_content_buffer) - 2)) return -1;

			Lxip::ssize_t res;

			enum { DEFAULT_BACKLOG = 5 };
			unsigned long result = DEFAULT_BACKLOG;
			Genode::ascii_to_unsigned(src, result, 10);

			res = _sc.listen(_handle, result);
			if (res != 0) return -1;

			Genode::memcpy(_content_buffer, src, len);
			_content_buffer[len+1] = '\n';
			_content_buffer[len+2] = '\0';

			_parent.listen(true);

			return len;
		}

		Lxip::ssize_t read(char *dst, Genode::size_t len, file_size seek_offset) override
		{
			if (seek_offset != 0) return -1;

			if (len < sizeof(_content_buffer)) return -1;

			Genode::size_t const n = Genode::strlen(_content_buffer);
			Genode::memcpy(dst, _content_buffer, n);

			return n;
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

		Lxip::ssize_t write(char const *src, Genode::size_t len, file_size seek_offset) override
		{
			if (seek_offset != 0)                    return -1;
			if (len > (sizeof(_content_buffer) - 2)) return -1;

			int res = _sc.dial(_handle, src);

			switch (res) {
			case Lxip::Io_result::LINUX_EINPROGRESS:
				_connecting = true;
				throw Would_block();
				break;
			case Lxip::Io_result::LINUX_EALREADY:
				if (!_connecting) return -1;
				throw Would_block();
				break;
			case Lxip::Io_result::LINUX_EISCONN:
				/*
				 * Connecting on an already connected socket is an error.
				 * If we get this error after we got EINPROGRESS it is
				 * fine.
				 */
				if (_is_connected || !_connecting) return -1;
				_is_connected = true;
				break;
			default:
				if (res != 0) return -1;
				break;
			}

			Genode::memcpy(_content_buffer, src, len);
			_content_buffer[len+1] = '\n';
			_content_buffer[len+2] = '\0';

			_parent.connect(true);

			return len;
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

		Lxip::ssize_t read(char *dst, Genode::size_t len, file_size seek_offset) override
		{
			if (seek_offset != 0)              return -1;
			if (len < sizeof(_content_buffer)) return -1;

			int res = _sc.local(_handle, _content_buffer, sizeof(_content_buffer));
			if (res < 0) return -1;

			return Genode::snprintf(dst, len, "%s\n", _content_buffer);
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

		Lxip::ssize_t read(char *dst, Genode::size_t len, file_size seek_offset) override
		{
			if (seek_offset != 0)              return -1;
			if (len < sizeof(_content_buffer)) return -1;

			int res = _sc.remote(_handle, _content_buffer, sizeof(_content_buffer));
			if (res < 0) return -1;

			return Genode::snprintf(dst, len, "%s\n", _content_buffer);
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

		Lxip::ssize_t read(char *dst, Genode::size_t len, file_size seek_offset) override
		{
			if (len < Lxip::MAX_FD_STR_LEN) return -1;

			Lxip::Handle h = _sc.accept(_handle, nullptr /* addr */, nullptr /* len */);
			if (!h.socket) return -1;

			/* check for EAGAIN */
			if (h.socket == (void*)0x1) throw Would_block();

			try {
				unsigned const id = _parent.accept(h);
				return Genode::snprintf(dst, len, "%s/%u", _parent.top_dir(), id);
			} catch (...) { PERR("Could not adopt new client socket"); }

			return -1;
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

		Lxip::ssize_t read(char *dst, Genode::size_t len, file_size seek_offset) override
		{
			/* TODO: IPv6 */
			struct sockaddr_storage addr;
			Lxip::uint32_t addr_len = sizeof(addr);

			/* peek to get the address of the currently queued data */
			Lxip::ssize_t res = _sc.recv(_handle, dst, 0, MSG_PEEK, 0 /* familiy */,
			                             &addr, &addr_len);
			if (res == Lxip::Io_result::LINUX_EAGAIN) throw Would_block();

			in_addr const   i_addr = ((struct sockaddr_in*)&addr)->sin_addr;
			unsigned char const *a = (unsigned char*)&i_addr.s_addr;
			unsigned char const *p = (unsigned char*)&((struct sockaddr_in*)&addr)->sin_port;

			/* TODO: big endian support */
			return Genode::snprintf(dst, len, "%d.%d.%d.%d:%u\n",
			                        a[0], a[1], a[2], a[3],
			                        (p[0]<<8)|(p[1]<<0));
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

		Lxip::ssize_t write(char const *src, Genode::size_t len, file_size seek_offset) override
		{
			sockaddr_in *to_addr = (sockaddr_in*)&_parent.to_addr();

			strncpy(_content_buffer, src, min(len+1, sizeof(_content_buffer)));

			to_addr->sin_port = htons(remove_port(_content_buffer));
			inet_pton(AF_INET, _content_buffer, &(to_addr->sin_addr));

			return len;
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
		: File("new_socket"), _parent(parent), _sc(sc) { }

		Lxip::ssize_t read(char *dst, Genode::size_t len, file_size seek_offset)
		{
			if (seek_offset != 0) return -1;

			if (len < Lxip::MAX_FD_STR_LEN) {
				Genode::error("Could not read new_socket file, buffer too small");
				return -1;
			}

			Lxip::Handle h = _sc.socket(_parent.type());
			if (!h.socket) return -1;

			h.non_block = true;

			try {
				unsigned const id = _parent.adopt_socket(h, false);
				return Genode::snprintf(dst, len, "%s/%u", _parent.top_dir(), id);
			} catch (...) {
				Genode::error("Could not adopt socket");
			}

			_sc.close(h);
			return -1;
		}
};


struct Vfs::Lxip_vfs_handle : Vfs::Vfs_handle
{
	Vfs::File *_file;

	Lxip_vfs_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                Vfs::File *file)
	: Vfs_handle(fs, fs, alloc, status_flags), _file(file) { }

	Vfs::File *file() { return _file; }
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
				*out_handle = new (alloc) Vfs::Lxip_vfs_handle(_parent, alloc, 0, file);
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


		/*************************************
		 ** Lxip_file I/O service interface **
		 *************************************/

		Vfs::File_io_service::Write_result
		write(Vfs::Vfs_handle *vfs_handle, char const *src,
		                   Vfs::file_size count,
		                   Vfs::file_size &out_count)
		{
			Vfs::File *file = static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle)->file();

			try {
				Lxip::ssize_t res = file->write(src, count, vfs_handle->seek());
				if (res < 0) return Vfs::File_io_service::WRITE_ERR_IO;

				out_count = res;
				return Vfs::File_io_service::WRITE_OK;

			} catch (Vfs::File::Would_block) {
				return Vfs::File_io_service::WRITE_ERR_WOULD_BLOCK; }
		}

		Vfs::File_io_service::Read_result
		read(Vfs::Vfs_handle *vfs_handle, char *dst,
		                 Vfs::file_size count,
		                 Vfs::file_size &out_count)
		{
			Vfs::File *file = static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle)->file();

			try {
				Lxip::ssize_t res = file->read(dst, count, vfs_handle->seek());
				if (res < 0) return Vfs::File_io_service::READ_ERR_IO;

				out_count = res;
				return Vfs::File_io_service::READ_OK;

			} catch (Vfs::File::Would_block) {
				return Vfs::File_io_service::READ_ERR_WOULD_BLOCK; }
		}
};


/*******************************
 ** Filesystem implementation **
 *******************************/

class Vfs::Lxip_file_system : public Vfs::File_system,
                              public Vfs::Directory
{
	private:

		Genode::Allocator &_alloc;

		Genode::Xml_node _config;
		char             _config_buf[128];

		char *_parse_config(Genode::Xml_node);

		/**
		 * Interface to LXIP stack
		 */
		struct Lxip::Socketcall &_socketcall { Lxip::init(_parse_config(_config)) };

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
			return (Genode::strcmp(path, "") == 0) || (Genode::strcmp(path, "/") == 0);
		}

	public:

		Lxip_file_system(Genode::Allocator &alloc, Genode::Xml_node config)
		: Directory(""), _alloc(alloc), _config(config)
		{
			_socketcall.register_ticker(vfs_tick);
		}

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
			out = Stat();

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
				*out_handle = new (alloc) Vfs::Lxip_vfs_handle(*this, alloc, 0, file);
				return OPEN_OK;
			}

			return OPEN_ERR_UNACCESSIBLE;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			if (!vfs_handle) return;

			Genode::destroy(vfs_handle->alloc(), vfs_handle);
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

		Write_result write(Vfs::Vfs_handle *vfs_handle, char const *src,
		                   Vfs::file_size count,
		                   Vfs::file_size &out_count) override
		{
			Vfs::File *file = static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle)->file();

			try {
				Lxip::ssize_t res = file->write(src, count, vfs_handle->seek());
				if (res < 0) return WRITE_ERR_IO;

				out_count = res;
				return WRITE_OK;

			} catch (File::Would_block) { return WRITE_ERR_WOULD_BLOCK; }
		}

		Read_result read(Vfs::Vfs_handle *vfs_handle, char *dst,
		                 Vfs::file_size count,
		                 Vfs::file_size &out_count) override
		{
			Vfs::File *file = static_cast<Vfs::Lxip_vfs_handle*>(vfs_handle)->file();

			try {
				Lxip::ssize_t res = file->read(dst, count, vfs_handle->seek());
				if (res < 0) return READ_ERR_IO;

				out_count = res;
				return READ_OK;

			} catch (File::Would_block) { return READ_ERR_WOULD_BLOCK; }
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			/* report ok because libc always executes ftruncate() when opening rw */
			return FTRUNCATE_OK;
		}
};


char *Vfs::Lxip_file_system::_parse_config(Genode::Xml_node config)
{
	char ip_addr_str[16] = {0};
	char netmask_str[16] = {0};
	char gateway_str[16] = {0};

	try {
		try {
			config.attribute("ip_addr").value(ip_addr_str, sizeof(ip_addr_str));
		} catch (...) { }

		try {
			config.attribute("netmask").value(netmask_str, sizeof(netmask_str));
		} catch (...) { }

		try {
			config.attribute("gateway").value(gateway_str, sizeof(gateway_str));
		} catch (...) { }

		/* either none or all 3 interface attributes must exist */
		if ((Genode::strlen(ip_addr_str) != 0) ||
		    (Genode::strlen(netmask_str) != 0) ||
		    (Genode::strlen(gateway_str) != 0)) {
			if (Genode::strlen(ip_addr_str) == 0) {
				PERR("Missing \"ip_addr\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			} else if (Genode::strlen(netmask_str) == 0) {
				PERR("Missing \"netmask\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			} else if (Genode::strlen(gateway_str) == 0) {
				PERR("Missing \"gateway\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			}
		} else throw -1;

		PLOG("static network interface: ip_addr=%s netmask=%s gateway=%s ",
		     ip_addr_str, netmask_str, gateway_str);

		Genode::snprintf(_config_buf, sizeof(_config_buf), "%s::%s:%s:::off",
		                 ip_addr_str, gateway_str, netmask_str);
	} catch (...) {
		PINF("Using DHCP for interface configuration.");
		Genode::strncpy(_config_buf, "dhcp", sizeof(_config_buf));
	}

	PLOG("init_libc_lxip() address config=%s\n", _config_buf);

	return _config_buf;
}


struct Lxip_factory : Vfs::File_system_factory
{
	Genode::Allocator &alloc;

	Lxip_factory(Genode::Allocator &alloc) : alloc(alloc) { }

	Vfs::File_system *create(Genode::Xml_node node) override
	{
		return new (&alloc) Vfs::Lxip_file_system(alloc, node);
	}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Lxip_factory factory(*Genode::env()->heap());
	return &factory;
}
