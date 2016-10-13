/*
 * \brief  LwIP VFS plugin
 * \author Emery Hemingway
 * \date   2016-09-27
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <vfs/directory_service.h>
#include <vfs/file_io_service.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>
#include <base/log.h>
#include <base/debug.h>

/* LwIP includes */
#include <lwip/nic_netif.h>

namespace Vfs {
	struct Node;
	struct File;
	struct Directory;
}

namespace Lwip {
extern "C" {
#include <lwip/udp.h>
#include <lwip/tcp.h>
}

	static Genode::Signal_context_capability read_ready_sigh_cap;

	using namespace Vfs;

	char const *get_port(char const *s)
	{
		char const *p = s;
		while (*++p) {
			if (*p == ':')
				return ++p;
		}
		return nullptr;
	}

	class Socket_dir;
	class Udp_socket_dir;
	class Tcp_socket_dir;
	typedef Genode::List<Udp_socket_dir> Udp_socket_dir_list;
	typedef Genode::List<Tcp_socket_dir> Tcp_socket_dir_list;

	struct Protocol_dir;
	template <typename, typename> class Protocol_dir_impl;

	enum {
		MAX_SOCKETS         = 128,      /* 3 */
		MAX_SOCKET_NAME_LEN = 3 + 1,    /* + \0 */
		MAX_FD_STR_LEN      = 3 + 1 +1, /* + \n + \0 */
		MAX_DATA_LEN        = 32,       /* 255.255.255.255:65536 + something */
	};

	struct Lwip_new_handle;
	struct Lwip_handle;
	typedef Genode::List<Lwip_handle> Lwip_handle_list;

	class File_system;

	typedef Vfs::Directory_service::Open_result Open_result;
	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Read_result   Read_result;

	extern "C" {
		static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
		static err_t tcp_connect_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
		static err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
		static err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb,
		                               struct pbuf *p, err_t err);
		static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len);
	}

	typedef Genode::Path<24> Path;

	enum {
		PORT_STRLEN_MAX = 6, /* :65536 */
		ENDPOINT_STRLEN_MAX = IPADDR_STRLEN_MAX+PORT_STRLEN_MAX
	};
}


struct Lwip::Lwip_new_handle final : Vfs::Vfs_handle
{
	Protocol_dir &proto_dir;

	Lwip_new_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	                Protocol_dir &p)
	: Vfs_handle(fs, fs, alloc, status_flags), proto_dir(p) { }
};


struct Lwip::Lwip_handle final : Vfs::Vfs_handle, Lwip_handle_list::Element
{
	enum Type {
		ACCEPT, BIND, CONNECT, DATA,
		LISTEN, LOCAL, REMOTE, INVALID
	};

	static Type type_from_name(Path const &p)
	{
		if (p == "/accept")
			return ACCEPT;
		else if (p == "/bind")
			return BIND;
		else if (p == "/connect")
			return CONNECT;
		else if (p == "/data")
			return DATA;
		else if (p == "/listen")
			return LISTEN;
		else if (p == "/local")
			return LOCAL;
		else if (p == "/remote")
			return REMOTE;
		return INVALID;
	}

	Socket_dir &socket;

	Type const type;

	Lwip_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	            Socket_dir &s, Type t)
	: Vfs_handle(fs, fs, alloc, status_flags), socket(s), type(t) { }
};


struct Lwip::Socket_dir
{
	public:

		typedef Genode::String<8> Name;

		static Name name_from_num(unsigned num)
		{
			char buf[Name::capacity()];
			return Name(Genode::Cstring(buf, Genode::snprintf(buf, Name::capacity(), "%x", num)));
		}

	private:

		unsigned const _num;
		Name     const _name { name_from_num(_num) };

	protected:

		/* queue of received data */
		pbuf *_recv_pbuf = NULL;
		u16_t _recv_off  = 0;

	public:

		/* lists of handles opened at this socket */
		Lwip_handle_list  accept_handles;
		Lwip_handle_list    bind_handles;
		Lwip_handle_list connect_handles;
		Lwip_handle_list    data_handles;
		Lwip_handle_list  listen_handles;
		Lwip_handle_list   local_handles;
		Lwip_handle_list  remote_handles;

		enum State {
			NEW,
			BOUND,
			CONNECT,
			LISTEN,
			READY,
			CLOSED
		};

		State state = NEW;

		Socket_dir(unsigned num) : _num(num) { };

		Name const &name() const { return _name; }

		bool operator == (unsigned other) const {
			return _num == other; }

		bool operator == (char const *other) const {
			return _name == other; }

		void close(Lwip_handle *handle)
		{
			typedef Lwip_handle::Type Type;
			switch (handle->type) {
			case Type::ACCEPT:   accept_handles.remove(handle); break;
			case Type::BIND:       bind_handles.remove(handle); break;
			case Type::CONNECT: connect_handles.remove(handle); break;
			case Type::DATA:       data_handles.remove(handle); break;
			case Type::LISTEN:   listen_handles.remove(handle); break;
			case Type::LOCAL:     local_handles.remove(handle); break;
			case Type::REMOTE:   remote_handles.remove(handle); break;
			case Type::INVALID: break;
			}
		}

		Open_result open(Vfs::File_system &fs,
		                 Path const  &name,
		                 unsigned     mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc)
		{
			typedef Lwip_handle::Type Type;

			Type type = Lwip_handle::type_from_name(name.base()+1);
			Lwip_handle *handle = (type != Type::INVALID)
				? new (alloc) Lwip_handle(fs, alloc, mode, *this, type)
				: nullptr;
			*out_handle = handle;

			switch (type) {
			case Type::ACCEPT:   accept_handles.insert(handle); break;
			case Type::BIND:       bind_handles.insert(handle); break;
			case Type::CONNECT: connect_handles.insert(handle); break;
			case Type::DATA:       data_handles.insert(handle); break;
			case Type::LISTEN:   listen_handles.insert(handle); break;
			case Type::LOCAL:     local_handles.insert(handle); break;
			case Type::REMOTE:   remote_handles.insert(handle); break;
			case Type::INVALID:
				Genode::error("invalid file ",name);
				return Open_result::OPEN_ERR_UNACCESSIBLE;
			}
			return Open_result::OPEN_OK;
		}

		/**
		 * Buffer received data and notify the application
		 */
		void recv(struct pbuf *buf)
		{
			/* chain to or replace the socket recv buffer */
			if (_recv_pbuf)
				pbuf_cat(_recv_pbuf, buf);
			else
				_recv_pbuf = buf;

			if (read_ready_sigh_cap.valid())
				Genode::Signal_transmitter(read_ready_sigh_cap).submit();
		}

		virtual Read_result   read(Lwip_handle &handle, char *dst,
		                           file_size count, file_size &out_count) = 0;
		virtual Write_result write(Lwip_handle &handle, char const *src,
		                           file_size count, file_size &out_count) = 0;

		virtual bool poll(Lwip_handle &handle) = 0;
};


struct Lwip::Protocol_dir
{
	virtual Socket_dir::Name const &alloc_socket() = 0;
	virtual Open_result open(Vfs::File_system &fs,
	                         char const  *path,
	                         unsigned     mode,
	                         Vfs_handle **out_handle,
	                         Allocator   &alloc) = 0;
};


template <typename SOCKET_DIR, typename PCB>
class Lwip::Protocol_dir_impl final : public Protocol_dir
{
	private:

		Genode::Allocator &_alloc;
		Genode::List<SOCKET_DIR> _socket_dirs;

	public:

		Protocol_dir_impl(Genode::Allocator &alloc) : _alloc(alloc) { }

		SOCKET_DIR *lookup(char const *name)
		{
			/* make sure it is only a name */
			for (char const *p = name; *p; ++p)
				if (*p == '/')
					return nullptr;

			for (SOCKET_DIR *sd = _socket_dirs.first(); sd; sd = sd->next())
				if (*sd == name)
					return sd;

			return nullptr;
		}

		bool leaf_path(char const *path)
		{
			Path subpath(path);
			subpath.strip_last_element();
			if ((subpath == "/") || (subpath == "/new_socket"))
				return true;
			if (lookup(subpath.base()+1)) {
				subpath.import(path);
				subpath.keep_only_last_element();
				return (Lwip_handle::type_from_name(subpath.base()+1) != Lwip_handle::INVALID);
			}
			return false;
		}

		Directory_service::Stat_result stat(char const *path, Directory_service::Stat &st)
		{
			Path subpath(path);

			if (subpath == "/") {
				st.size = 1;
				st.mode = Directory_service::STAT_MODE_DIRECTORY;
				st.inode = (Genode::addr_t)this;
				return Directory_service::STAT_OK;
			}

			if (subpath == "/new_socket") {
				st.mode = Directory_service::STAT_MODE_CHARDEV;
				st.inode = ((Genode::addr_t)this)+1;
				return Directory_service::STAT_OK;
			}

			subpath.strip_last_element();
			if (SOCKET_DIR *dir = lookup(subpath.base()+1)) {
				Path filename(path);
				filename.keep_only_last_element();
				if (filename == subpath.base()) {
					st.size = Lwip_handle::INVALID;
					st.mode = Directory_service::STAT_MODE_DIRECTORY;
					st.inode = (Genode::addr_t)dir;
					return Directory_service::STAT_OK;
				}

				Lwip_handle::Type t = Lwip_handle::type_from_name(filename.base()+1);
				if (t != Lwip_handle::INVALID) {
					st.mode = Directory_service::STAT_MODE_CHARDEV;
					st.inode = ((Genode::addr_t)dir)+t;
					return Directory_service::STAT_OK;
				}
			}
			return Directory_service::STAT_ERR_NO_ENTRY;
		}

		Socket_dir::Name const &alloc_socket() override
		{
			/*
			 * use the equidistribution RNG to hide the socket count,
			 * see src/lib/lwip/platform/rand.cc
			 */
			unsigned id = LWIP_RAND();

			/* check for collisions */
			for (SOCKET_DIR *dir = _socket_dirs.first(); dir;) {
				if (*dir == id) {
					id = LWIP_RAND();
					dir = _socket_dirs.first();
				} else {
					dir = dir->next();
				}
			}

			SOCKET_DIR *new_socket = new (_alloc) SOCKET_DIR(id, *this);
			_socket_dirs.insert(new_socket);
			return new_socket->name();
		}

		Socket_dir::Name const &alloc_socket(PCB *pcb)
		{
			/*
			 * use the equidistribution RNG to hide the socket count,
			 * see src/lib/lwip/platform/rand.cc
			 */
			unsigned id = LWIP_RAND();

			/* check for collisions */
			for (SOCKET_DIR *dir = _socket_dirs.first(); dir;) {
				if (*dir == id) {
					id = LWIP_RAND();
					dir = _socket_dirs.first();
				} else {
					dir = dir->next();
				}
			}

			SOCKET_DIR *new_socket = new (_alloc) SOCKET_DIR(id, *this, pcb);
			_socket_dirs.insert(new_socket);
			return new_socket->name();
		}

		Open_result open(Vfs::File_system &fs,
		                 char const  *path,
		                 unsigned     mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			Path subpath(path);

			if (subpath == "/new_socket") {
				*out_handle = new (alloc) Lwip_new_handle(fs, alloc, mode, *this);
				return Open_result::OPEN_OK;
			}

			subpath.strip_last_element();
			if (SOCKET_DIR *dir = lookup(subpath.base()+1)) {
				subpath.import(path);
				subpath.keep_only_last_element();
				return dir->open(fs, subpath, mode, out_handle, alloc);
			}

			return Open_result::OPEN_ERR_UNACCESSIBLE;
		}
};

namespace Lwip {
	typedef Protocol_dir_impl<Udp_socket_dir, udp_pcb> Udp_proto_dir;
	typedef Protocol_dir_impl<Tcp_socket_dir, tcp_pcb> Tcp_proto_dir;
}


/*********
 ** UDP **
 *********/

class Lwip::Udp_socket_dir final :
	public Socket_dir,
	public Udp_socket_dir_list::Element
{
	private:

		udp_pcb *_pcb = udp_new();

	public:

		Udp_socket_dir(unsigned num, Udp_proto_dir&) : Socket_dir(num) { }

		~Udp_socket_dir() { udp_remove(_pcb); }

		void ready_callbacks()
		{
			/* 'this' will be the argument to the LwIP callback */
			udp_recv(_pcb, udp_recv_callback, this);
		}


		/**************************
		 ** Socket_dir interface **
		 **************************/

		bool poll(Lwip_handle &handle) override
		{
			typedef Lwip_handle::Type Type;

			switch(handle.type) {
			case Type::DATA:
				return _recv_pbuf ? _recv_pbuf->len : false;
			default:
				return false;
			}
		}

		Read_result read(Lwip_handle &handle, char *dst,
		                 file_size count, file_size &out_count) override
		{
			typedef Lwip_handle::Type Type;

			switch(handle.type) {
			case Type::ACCEPT:  PDBG("udp read ACCEPT"); break;
			case Type::BIND:    PDBG("udp read BIND"); break;
			case Type::CONNECT: PDBG("udp read CONNECT"); break;

			case Type::DATA:
				if (state == READY) {
					if (!_recv_pbuf)
						return Read_result::READ_ERR_WOULD_BLOCK;

					/* copy to application buffer */
					u16_t n = out_count = pbuf_copy_partial(
						_recv_pbuf, dst, count, _recv_off);

					/* move our pbuf reference down the chain and free the head */
					pbuf *head = _recv_pbuf;
					_recv_pbuf = pbuf_skip(_recv_pbuf, n, &_recv_off);
					if (_recv_pbuf)
						pbuf_ref(_recv_pbuf);
					pbuf_free(head);
					return Read_result::READ_OK;
				}
				break;

			case Type::LISTEN: PDBG("udp read LISTEN"); break;
			case Type::LOCAL:
				if (state == READY) {
					char const *ip_str = ipaddr_ntoa(&_pcb->local_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                             ip_str, _pcb->local_port);
					return Read_result::READ_OK;
				}
				break;

			case Type::REMOTE:
				if (state == READY) {
					char const *ip_str = ipaddr_ntoa(&_pcb->remote_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                             ip_str, _pcb->remote_port);
					return Read_result::READ_OK;
				}
				break;
			case Type::INVALID: break;
			}
			return Read_result::READ_ERR_INVALID;
		}

		Write_result write(Lwip_handle &handle, char const *src,
		                   file_size count, file_size &out_count) override
		{
			typedef Lwip_handle::Type Type;

			switch(handle.type) {
			case Type::ACCEPT: break;
			case Type::BIND:
				if ((state == NEW) && (count < ENDPOINT_STRLEN_MAX)) {
					char buf[ENDPOINT_STRLEN_MAX];
					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					ip_addr_t addr;
					u16_t port = LWIP_RAND();

					if (!ipaddr_aton(buf, &addr))
						return Write_result::WRITE_ERR_INVALID;
					Genode::ascii_to_unsigned(get_port(buf), port, 10);

					err_t err = udp_bind(_pcb, &addr, port);
					if (err == ERR_OK) {
						state = BOUND;
						return Write_result::WRITE_OK;
					}
					return Write_result::WRITE_ERR_IO;
				}
				break;

			case Type::CONNECT:
				if (((state == BOUND) || (state == NEW)) && (count < ENDPOINT_STRLEN_MAX)) {
					char buf[ENDPOINT_STRLEN_MAX];
					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					ip_addr_t addr;
					u16_t port = 0;

					if (!ipaddr_aton(buf, &addr))
						return Write_result::WRITE_ERR_INVALID;
					Genode::ascii_to_unsigned(get_port(buf), port, 10);

					err_t err = udp_connect(_pcb, &addr, port);
					if (err != ERR_OK)
						return Write_result::WRITE_ERR_IO;

					state = READY;
					ready_callbacks();
					return Write_result::WRITE_OK;
				}
				break;

			case Type::DATA:
				if (state == READY) {
					pbuf *head = pbuf_alloc(PBUF_RAW, count, PBUF_POOL);
					if (err_t err = pbuf_take(head, src, count) != ERR_OK) {
						Genode::error("error copying send data to LwIP buffer, ",(int)err);
						return Write_result::WRITE_ERR_IO;
					}

					if (udp_send(_pcb, head) != ERR_OK)
						return Write_result::WRITE_ERR_IO;

					out_count = head->tot_len;
					return Write_result::WRITE_OK;
				}
				break;

			case Type::LISTEN:
			case Type::LOCAL:
			case Type::REMOTE:
			case Type::INVALID: break;
			}
			return Write_result::WRITE_ERR_INVALID;
		}

};


/*********
 ** TCP **
 *********/

class Lwip::Tcp_socket_dir final :
	public Socket_dir,
	public Tcp_socket_dir_list::Element
{
	private:

		Tcp_proto_dir &_proto_dir;

		tcp_pcb *_pcb;

	public:

		Tcp_socket_dir(unsigned num, Tcp_proto_dir &proto_dir)
		: Socket_dir(num), _proto_dir(proto_dir), _pcb(tcp_new())
		{
			/* 'this' will be the argument to LwIP callbacks */
			tcp_arg(_pcb, this);
		}

		Tcp_socket_dir(unsigned num, Tcp_proto_dir &proto_dir, tcp_pcb *pcb)
		: Socket_dir(num), _proto_dir(proto_dir), _pcb(pcb)
		{
			/* 'this' will be the argument to LwIP callbacks */
			tcp_arg(_pcb, this);

			/* the PCB comes connected and ready to use */
			state = READY;
			ready_callbacks();
		}

		~Tcp_socket_dir() { tcp_close(_pcb); }

		void ready_callbacks()
		{
			tcp_recv(_pcb, tcp_recv_callback);
			tcp_sent(_pcb, tcp_sent_callback);
		}

		/**
		 * Accept new connection from callback
		 */
		err_t accept(struct tcp_pcb *newpcb, err_t err)
		{
			Socket_dir::Name const &id = _proto_dir.alloc_socket(newpcb);

			/*
			handle->read_callback("/tcp/", 5, Callback::PARTIAL);
			handle->read_callback(id.string(), id.length()-1, Callback::PARTIAL);
			handle->read_callback("\n", 1, Callback::COMPLETE);
			 */

			Genode::error("leaking accepted TCP connection, ",__FILE__,":",__LINE__);

			if (read_ready_sigh_cap.valid())
				Genode::Signal_transmitter(read_ready_sigh_cap).submit();
			return ERR_OK;
		}

		/**************************
		 ** Socket_dir interface **
		 **************************/

		bool poll(Lwip_handle &handle) override
		{
			typedef Lwip_handle::Type Type;

			switch(handle.type) {
			case Type::DATA:
				return _recv_pbuf ? _recv_pbuf->len : false;
			default:
				return false;
			}
		}

		Read_result read(Lwip_handle &handle, char *dst,
		                 file_size count, file_size &out_count) override
		{
			typedef Lwip_handle::Type Type;

			switch(handle.type) {
			case Type::ACCEPT: PDBG("tcp read ACCEPT not implemented"); 
			case Type::BIND:    break;
			case Type::CONNECT: break;

			case Type::DATA:
				if (state == READY) {
					if (!_recv_pbuf)
						return Read_result::READ_ERR_WOULD_BLOCK;

					/* copy to application buffer */
					u16_t n = out_count = pbuf_copy_partial(
						_recv_pbuf, dst, count, _recv_off);

					/* move our pbuf reference down the chain and free the head */
					pbuf *head = _recv_pbuf;
					_recv_pbuf = pbuf_skip(_recv_pbuf, n, &_recv_off);
					if (_recv_pbuf)
						pbuf_ref(_recv_pbuf);
					pbuf_free(head);

					/* ACK the remote */
					tcp_recved(_pcb, n);
					return Read_result::READ_OK;
				}
				break;

			case Type::LISTEN: {
			}
			case Type::LOCAL:
				if (state == READY) {
					char const *ip_str = ipaddr_ntoa(&_pcb->local_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                               ip_str, _pcb->local_port);
					return Read_result::READ_OK;
				}
				break;

			case Type::REMOTE:
				if (state == READY) {
					char const *ip_str = ipaddr_ntoa(&_pcb->remote_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                             ip_str, _pcb->remote_port);
					return Read_result::READ_OK;
				}
				break;
			case Type::INVALID: break;
			}
			return Read_result::READ_ERR_INVALID;
		}

		Write_result write(Lwip_handle &handle, char const *src,
		                   file_size count, file_size &out_count) override
		{
			typedef Lwip_handle::Type Type;

			switch(handle.type) {
			case Type::ACCEPT: break;
			case Type::BIND:
				if ((state == NEW) && (count < ENDPOINT_STRLEN_MAX)) {
					char buf[ENDPOINT_STRLEN_MAX];
					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					ip_addr_t addr;
					u16_t port = 0;

					for (Genode::size_t i = count-2; i >= 0; --i) {
						if (buf[i] == ':') {
							buf[i++] = '\0';
							Genode::ascii_to_unsigned(&buf[i], port, 10);
							break;
						}
					}

					if (!ipaddr_aton(buf, &addr))
						return Write_result::WRITE_ERR_INVALID;

					if (tcp_bind(_pcb, &addr, port) == ERR_OK) {
						state = BOUND;
						return Write_result::WRITE_OK;
					}
					return Write_result::WRITE_ERR_IO;
				}
				break;

			case Type::CONNECT:
				if ((state == BOUND) && (count < ENDPOINT_STRLEN_MAX)) {
					char buf[ENDPOINT_STRLEN_MAX];
					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					ip_addr_t addr;
					u16_t port = 0;

					if (!ipaddr_aton(buf, &addr))
						return Write_result::WRITE_ERR_INVALID;
					Genode::ascii_to_unsigned(get_port(buf), port, 10);

					err_t err = tcp_connect(_pcb, &addr, port, tcp_connect_callback);
					return err == ERR_OK ?
						Write_result::WRITE_OK : Write_result::WRITE_ERR_IO;
				}
				break;

			case Type::DATA:
				if (state == READY) {
					/* write to outgoing TCP buffer */
					if (tcp_write(_pcb, src, count, 0) != ERR_OK) {
						return Write_result::WRITE_ERR_IO;
					}

					/* flush the buffer */
					if (tcp_output(_pcb) != ERR_OK) {
						return Write_result::WRITE_ERR_IO;
					}

					/* TODO: the write should only complete when the remote ACKs */
					return Write_result::WRITE_OK;
				}
				break;

			case Type::LISTEN:
				if ((state == BOUND) && (count < 7)) {
					unsigned long backlog = TCP_DEFAULT_LISTEN_BACKLOG;
					char buf[8];
					Genode::strncpy(buf, src, min(count-1, sizeof(buf)));
					Genode::ascii_to_unsigned(buf, backlog, 10);

					/* this replaces the PCB so set the callbacks again */
					_pcb = tcp_listen_with_backlog(_pcb, backlog);
					tcp_arg(_pcb, this);
					tcp_accept(_pcb, tcp_accept_callback);
					state = LISTEN;
					return Write_result::WRITE_OK;
				}
				break;

			case Type::LOCAL:
			case Type::REMOTE:
			case Type::INVALID: break;
			}
			return Write_result::WRITE_ERR_INVALID;
		}
};


/********************
 ** LwIP callbacks **
 ********************/

namespace Lwip {
	extern "C" {

static
void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	Lwip::Udp_socket_dir *socket_dir = static_cast<Lwip::Udp_socket_dir *>(arg);
	socket_dir->recv(p);
}


static
err_t tcp_connect_callback(void *arg, struct tcp_pcb *pcb, err_t err)
{
	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);

	Genode::error("received connect callback, but must discard PCB ",__FILE__,":",__LINE__);

	return ERR_OK;
}


static
err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	return socket_dir->accept(newpcb, err);
};


static
err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	socket_dir->recv(p);
	return ERR_OK;
}


static
err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	Genode::error("received TCP sent callback ",__FILE__,":",__LINE__);

	return ERR_OK;
}

	}
}


/*********************
 ** VFS file-system **
 *********************/

class Lwip::File_system final : public Vfs::File_system
{
	private:

		Lwip::Nic_netif _netif;

		Tcp_proto_dir _tcp_dir;
		Udp_proto_dir _udp_dir;

		Genode::Entrypoint &_ep;

	public:

		File_system(Genode::Env &env,
		                        Genode::Allocator &alloc,
		                        Genode::Xml_node config)
		: _netif(env, alloc, config), _tcp_dir(alloc), _udp_dir(alloc), _ep(env.ep())
		{
			/* XXX: set _netif as the default interface */
		}

		~File_system() { }


		/***********************
		 ** Directory_service **
		 ***********************/

		char const *leaf_path(char const *path) override
		{
			if (strncmp(path, "/tcp", 4) == 0)
				return _tcp_dir.leaf_path(path+4) ? path : nullptr;
			if (strncmp(path, "/udp", 4) == 0)
				return _udp_dir.leaf_path(path+4) ? path : nullptr;
			return nullptr;
		}

		Stat_result stat(char const *path, Stat &st) override
		{
			st = Stat();
			st.device = (Genode::addr_t)this;

			Stat_result r = STAT_ERR_NO_PERM;

			if (strncmp(path, "/tcp", 4) == 0)
				r = _tcp_dir.stat(path+4, st);
			else if (strncmp(path, "/udp", 4) == 0)
				r = _udp_dir.stat(path+4, st);
			if (r != STAT_OK)
				Genode::error("stat of ",path," failed");
			else
				Genode::log("stat of ",path," succedded");
			return r;
			//return STAT_ERR_NO_PERM;
		}

		bool directory(char const *path) override
		{
			char const *subpath = path+4;
			if (strncmp(path, "/tcp", 4) == 0)
				return (*subpath) ? (_tcp_dir.lookup(subpath+1) != nullptr) : true;
			if (strncmp(path, "/udp", 4) == 0)
				return (*subpath) ? (_udp_dir.lookup(subpath+1) != nullptr) : true;
			return false;
		}

		Open_result open(char const  *path,
		                 unsigned     mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (mode & OPEN_MODE_CREATE) return OPEN_ERR_NO_PERM;

			if (strncmp(path, "/tcp", 4) == 0)
				return _tcp_dir.open(*this, path+4, mode, out_handle, alloc);
			if (strncmp(path, "/udp", 4) == 0)
				return _udp_dir.open(*this, path+4, mode, out_handle, alloc);
			return OPEN_ERR_UNACCESSIBLE;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			if (Lwip_new_handle *handle = dynamic_cast<Lwip_new_handle*>(vfs_handle))
				destroy(vfs_handle->alloc(), handle);
			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle)) {
				handle->socket.close(handle);
				destroy(vfs_handle->alloc(), handle);
			}
				
		}

		Unlink_result unlink(char const *path) override
		{
			PDBG("not implemented");
			return UNLINK_ERR_NO_PERM;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		File_io_service::Write_result
		write(Vfs_handle *vfs_handle, char const *src,
		      file_size count, file_size &out_count) override
		{
			if ((!vfs_handle) ||
			    ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_RDONLY))
				return WRITE_ERR_INVALID;

			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle))
				return handle->socket.write(*handle, src, count, out_count);

			return WRITE_ERR_INVALID;
		}

		File_io_service::Read_result
		read(Vfs_handle *vfs_handle, char *dst,
		     file_size count, file_size &out_count) override
		{
			if ((!vfs_handle) ||
			    ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_WRONLY))
				return READ_ERR_INVALID;

			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle)) {
				return handle->socket.read(*handle, dst, count, out_count);
			} else if (Lwip_new_handle *handle = dynamic_cast<Lwip_new_handle*>(vfs_handle)) {
				if (count < (Socket_dir::Name::capacity()+1))
					return READ_ERR_INVALID;
				try {
					Socket_dir::Name const &id = handle->proto_dir.alloc_socket();

					if (dynamic_cast<Tcp_proto_dir*>(&handle->proto_dir))
						Genode::snprintf(dst, count, "/tcp/%s\n", id.string());
					else if (dynamic_cast<Udp_proto_dir*>(&handle->proto_dir))
						Genode::snprintf(dst, count, "/udp/%s\n", id.string());
					return READ_OK;
				} catch (...) {
					Genode::error("failed to allocate new LwIP socket");
					return READ_ERR_IO;
				}
			}

			return READ_ERR_INVALID;
		}


		/***********************
		 ** File system stubs **
		 ***********************/

		Dirent_result dirent(char const *path, file_offset index, Dirent &) override {
			return DIRENT_ERR_NO_PERM; }

		Readlink_result readlink(const char*, char*, Vfs::file_size, Vfs::file_size&) override {
			return READLINK_ERR_NO_PERM; }

		Rename_result rename(char const *from, char const *to) override {
			return RENAME_ERR_NO_PERM; }

		Mkdir_result mkdir(char const *path, unsigned mode) override {
			return MKDIR_ERR_NO_PERM; }

		Symlink_result symlink(const char*, const char*) override {
			return SYMLINK_ERR_NO_PERM; }

		file_size num_dirent(char const *path) override {
			return 0; }

		Dataspace_capability dataspace(char const *path) override {
			return Dataspace_capability(); }
		void release(char const *path, Dataspace_capability) override { };

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			/* report ok because libc always executes ftruncate() when opening rw */
			return FTRUNCATE_OK;
		}

		void read_ready_sigh(Genode::Signal_context_capability sigh) override
		{
			read_ready_sigh_cap = sigh;
		}

		void notify_read_ready(Vfs_handle *vfs_handle) override
		{
			PDBG("not implemented");
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			Lwip_handle *handle =
				static_cast<Lwip_handle *>(vfs_handle);

			/* TODO when to _polling_handles.remove(handle); ? */
			return handle->socket.poll(*handle);
		}
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Genode::Env &env,
		                         Genode::Allocator &alloc,
		                         Genode::Xml_node config) override
		{
			return new (alloc) Lwip::File_system(env, alloc, config);
		}
	};

	static Factory f;
	return &f;
}
