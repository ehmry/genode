/*
 * \brief  LwIP VFS plugin
 * \author Emery Hemingway
 * \date   2016-09-27
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vfs/directory_service.h>
#include <vfs/file_io_service.h>
#include <vfs/file_system_factory.h>
#include <vfs/vfs_handle.h>
#include <timer_session/connection.h>
#include <util/fifo.h>
#include <base/tslab.h>
#include <base/log.h>

/* LwIP includes */
#include <lwip/genode_init.h>
#include <lwip/nic_netif.h>

namespace Lwip {
extern "C" {
#include <lwip/udp.h>
#include <lwip/tcp.h>
}

	using namespace Vfs;
	typedef Vfs::File_io_service::Read_result Read_result;
	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Sync_result Sync_result;

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
	typedef Vfs::Directory_service::Unlink_result Unlink_result;

	extern "C" {
		static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
		static err_t tcp_connect_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
		static err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
		static err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb,
		                               struct pbuf *p, err_t err);
		/* static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len); */
		static void  tcp_err_callback(void *arg, err_t err);
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
		     if (p == "/accept")  return ACCEPT;
		else if (p == "/bind")    return BIND;
		else if (p == "/connect") return CONNECT;
		else if (p == "/data")    return DATA;
		else if (p == "/listen")  return LISTEN;
		else if (p == "/local")   return LOCAL;
		else if (p == "/remote")  return REMOTE;
		return INVALID;
	}

	Socket_dir *socket;

	int in_transit = 0;

	Type const type;

	bool notify = false;

	void print(Genode::Output &output) const;

	Lwip_handle(Vfs::File_system &fs, Allocator &alloc, int status_flags,
	            Socket_dir &s, Type t)
	: Vfs_handle(fs, fs, alloc, status_flags), socket(&s), type(t) { }
};

struct Lwip::Socket_dir
{
		typedef Genode::String<8> Name;

		static Name name_from_num(unsigned num)
		{
			char buf[Name::capacity()];
			return Name(Genode::Cstring(buf, Genode::snprintf(buf, Name::capacity(), "%x", num)));
		}

		Genode::Allocator &alloc;

		Vfs::Io_response_handler &io_handler;

		unsigned const _num;
		Name     const _name { name_from_num(_num) };

		/* lists of handles opened at this socket */
		Lwip_handle_list accept_handles;
		Lwip_handle_list   bind_handles;
		Lwip_handle_list   data_handles;
		Lwip_handle_list listen_handles;
		Lwip_handle_list remote_handles;

		enum State {
			NEW,
			BOUND,
			CONNECT,
			LISTEN,
			READY,
			CLOSING,
			CLOSED
		};

		Socket_dir(unsigned num, Genode::Allocator &alloc, Vfs::Io_response_handler &io_handler)
		: alloc(alloc), io_handler(io_handler), _num(num) { };

		Name const &name() const { return _name; }

		bool operator == (unsigned other) const {
			return _num == other; }

		bool operator == (char const *other) const {
			return _name == other; }

		void close(Lwip_handle *handle)
		{
			typedef Lwip_handle::Type Type;
			switch (handle->type) {
			case Type::ACCEPT: accept_handles.remove(handle); break;
			case Type::BIND:     bind_handles.remove(handle); break;
			case Type::DATA:     data_handles.remove(handle); break;
			case Type::LISTEN: listen_handles.remove(handle); break;
			case Type::REMOTE: remote_handles.remove(handle); break;

			case Type::CONNECT:
			case Type::LOCAL:
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
			case Type::ACCEPT: accept_handles.insert(handle); break;
			case Type::BIND:     bind_handles.insert(handle); break;
			case Type::DATA:     data_handles.insert(handle); break;
			case Type::LISTEN: listen_handles.insert(handle); break;
			case Type::REMOTE: remote_handles.insert(handle); break;

			case Type::CONNECT:
			case Type::LOCAL:
				break;

			case Type::INVALID:
				Genode::error("invalid file ",name);
				return Open_result::OPEN_ERR_UNACCESSIBLE;
			}

			return Open_result::OPEN_OK;
		}

		virtual Read_result read(Lwip_handle &handle,
		                         char *dst, file_size count,
		                         file_size &out_count) = 0;

		virtual Write_result write(Lwip_handle &handle,
		                           char const *src, file_size count,
		                           file_size &out_count) = 0;

		virtual bool read_ready(Lwip_handle&) = 0;

		void handle_io(Lwip::Lwip_handle_list &list)
		{
			Lwip::Lwip_handle *h = list.first();
			io_handler.handle_io_response(h ? h->context : nullptr);
		}

		virtual Sync_result complete_sync() = 0;
};


void Lwip::Lwip_handle::print(Genode::Output &output) const
{
		output.out_string(socket->name().string());
		switch (type) {
		case Type::ACCEPT:  output.out_string("/accept"); break;
		case Type::BIND:    output.out_string("/bind"); break;
		case Type::DATA:    output.out_string("/data"); break;
		case Type::LISTEN:  output.out_string("/listen"); break;
		case Type::REMOTE:  output.out_string("/remote"); break;
		case Type::CONNECT: output.out_string("/connect"); break;
		case Type::LOCAL:   output.out_string("/local"); break;
		case Type::INVALID: output.out_string("/invalid"); break;
		}
}


struct Lwip::Protocol_dir
{
	virtual Socket_dir::Name const &alloc_socket(Genode::Allocator&) = 0;
	virtual void adopt_socket(Socket_dir &socket) = 0;
	virtual Open_result open(Vfs::File_system &fs,
	                         char const  *path,
	                         unsigned     mode,
	                         Vfs_handle **out_handle,
	                         Allocator   &alloc) = 0;
	virtual Unlink_result unlink(char const *path) = 0;
};


template <typename SOCKET_DIR, typename PCB>
class Lwip::Protocol_dir_impl final : public Protocol_dir
{
	private:

		Genode::Allocator        &_alloc;
		Vfs::Io_response_handler &_io_handler;
		Genode::Entrypoint       &_ep;

		Genode::List<SOCKET_DIR> _socket_dirs;

	public:

		Protocol_dir_impl(Genode::Allocator        &alloc,
		                  Vfs::Io_response_handler &io_handler,
		                  Genode::Entrypoint       &ep)
		: _alloc(alloc), _io_handler(io_handler), _ep(ep) { }

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

			if (!subpath.has_single_element())
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

		Socket_dir::Name const &alloc_socket(Genode::Allocator &alloc) override
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

			SOCKET_DIR *new_socket = new (alloc)
				SOCKET_DIR(id, *this, alloc, _io_handler, _ep);
			_socket_dirs.insert(new_socket);
			return new_socket->name();
		}

		void adopt_socket(Socket_dir &socket) override {
			_socket_dirs.insert(static_cast<SOCKET_DIR*>(&socket)); }

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

		Unlink_result unlink(char const *path) override
		{
			Path subpath(path);

			if (!subpath.has_single_element()) {
				subpath.strip_last_element();
				return (lookup(subpath.base()+1) != nullptr)
					? Unlink_result::UNLINK_ERR_NO_PERM
					: Unlink_result::UNLINK_ERR_NO_ENTRY;
			}

			if (subpath == "/new_socket")
				return Unlink_result::UNLINK_ERR_NO_PERM;

			if (SOCKET_DIR *dir = lookup(subpath.base()+1)) {
				_socket_dirs.remove(dir);
				destroy(dir->alloc, dir);
				return Unlink_result::UNLINK_OK;
			}

			return Unlink_result::UNLINK_ERR_NO_ENTRY;
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

		/* TODO: optimize packet queue metadata allocator */
		Genode::Allocator &_alloc;

		udp_pcb *_pcb = udp_new();

		/**
		 * Handle on a received UDP packet
		 */
		struct Packet : Genode::Fifo<Packet>::Element
		{
			ip_addr_t const addr;
			u16_t     const port;
			pbuf           *buf;

			Packet(ip_addr_t const *addr, u16_t port, pbuf *buf)
			: addr(*addr), port(port), buf(buf) { }
		};

		Genode::Tslab<Packet, sizeof(Packet)*64> _packet_slab { &_alloc };

		/* Queue of received UDP packets */
		Genode::Fifo<Packet> _packet_queue;

		/* destination addressing */
		ip_addr_t _to_addr;
		u16_t     _to_port = 0;

	public:

		Udp_socket_dir(unsigned num, Udp_proto_dir &proto_dir,
		               Genode::Allocator &alloc,
		               Vfs::Io_response_handler &io_handler,
		               Genode::Entrypoint &ep)
		: Socket_dir(num, alloc, io_handler), _alloc(alloc)
		{
			ip_addr_set_zero(&_to_addr);

			/* 'this' will be the argument to the LwIP recv callback */
			udp_recv(_pcb, udp_recv_callback, this);
		}

		~Udp_socket_dir()
		{
			udp_remove(_pcb);
			_pcb = NULL;
		}

		/**
		 * Stuff a packet in the queue and notify the handle
		 */
		void queue(ip_addr_t const *addr, u16_t port, pbuf *buf)
		{
			try {
				Packet *pkt = new (_packet_slab) Packet(addr, port, buf);
				_packet_queue.enqueue(pkt);
			} catch (...) {
				Genode::warning("failed to queue UDP packet, dropping");
				pbuf_free(buf);
			}

			handle_io(remote_handles);
			handle_io(data_handles);
		}


		/**************************
		 ** Socket_dir interface **
		 **************************/

		bool read_ready(Lwip_handle &handle) override
		{
			typedef Lwip_handle::Type Type;
			switch (handle.type) {
			case Type::DATA:
			case Type::REMOTE:
				return !_packet_queue.empty();
			default:
				break;
			}
			return true;
		}

		Read_result read(Lwip_handle &handle,
		                 char *dst, file_size count,
		                 file_size &out_count) override
		{
			typedef Lwip_handle::Type Type;

			switch(handle.type) {

			case Type::DATA: {
				if (Packet *pkt = _packet_queue.head()) {
					auto n = pbuf_copy_partial(
						pkt->buf, dst, count, 0);

					if (n < pkt->buf->len) {
						/* use a header shift to hide what has been read */
						pbuf_header(pkt->buf, n);
					} else {
						pbuf_free(pkt->buf);
						destroy(_packet_slab, _packet_queue.dequeue());
					}
					out_count = n;
					return Read_result::READ_OK;
				}
				return Read_result::READ_QUEUED;
			}

			case Type::LOCAL:
			case Type::BIND: {
				if (count < ENDPOINT_STRLEN_MAX)
					return Read_result::READ_ERR_INVALID;
				char const *ip_str = ipaddr_ntoa(&_pcb->local_ip);
				/* TODO: [IPv6]:port */
				out_count = Genode::snprintf(dst, count, "%s:%d\n",
				                             ip_str, _pcb->local_port);
				return Read_result::READ_OK;
			}

			case Type::CONNECT: {
				/* check if the PCB was connected */
				if (ip_addr_isany(&_pcb->remote_ip))
					return Read_result::READ_OK;
				/* otherwise fallthru to REMOTE*/
			}

			case Type::REMOTE: {
				if (count < ENDPOINT_STRLEN_MAX) {
					Genode::error("VFS LwIP: accept file read buffer is too small");
					return Read_result::READ_ERR_INVALID;
				}
				if (ip_addr_isany(&_pcb->remote_ip)) {
					if (Packet *pkt = _packet_queue.head()) {
						char const *ip_str = ipaddr_ntoa(&pkt->addr);
						/* TODO: IPv6 */
						out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                                 ip_str, pkt->port);
						return Read_result::READ_OK;
					}
				} else {
					char const *ip_str = ipaddr_ntoa(&_pcb->remote_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                             ip_str, _pcb->remote_port);
					return Read_result::READ_OK;
				}
				break;
			}

			default: break;
			}

			return Read_result::READ_ERR_INVALID;
		}

		Write_result write(Lwip_handle &handle,
		                   char const *src, file_size count,
		                   file_size &out_count) override
		{
			typedef Lwip_handle::Type Type;

			switch(handle.type) {

			case Type::DATA: {
				file_size remain = count;
				while (remain) {
					pbuf *buf = pbuf_alloc(PBUF_RAW, remain, PBUF_POOL);
					pbuf_take(buf, src, buf->tot_len);

					err_t err = udp_sendto(_pcb, buf, &_to_addr, _to_port);
					pbuf_free(buf);
					if (err != ERR_OK)
						return Write_result::WRITE_ERR_IO;
					remain -= buf->tot_len;
					src += buf->tot_len;
				}
				out_count = count;
				return Write_result::WRITE_OK;
			}

			case Type::REMOTE: {
				if (!ip_addr_isany(&_pcb->remote_ip)) {
					return Write_result::WRITE_ERR_INVALID;
				} else {
					char buf[ENDPOINT_STRLEN_MAX];
					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					_to_port = remove_port(buf);
					out_count = count;
					if (ipaddr_aton(buf, &_to_addr)) {
						out_count = count;
						return Write_result::WRITE_OK;
					}
				}
				break;
			}

			case Type::BIND: {
				if (count < ENDPOINT_STRLEN_MAX) {
					char buf[ENDPOINT_STRLEN_MAX];
					ip_addr_t addr;
					u16_t port;

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));
					port = remove_port(buf);
					if (!ipaddr_aton(buf, &addr))
						break;

					err_t err = udp_bind(_pcb, &addr, port);
					if (err == ERR_OK) {
						out_count = count;
						return Write_result::WRITE_OK;
					}
					return Write_result::WRITE_ERR_IO;
				}
				break;
			}

			case Type::CONNECT: {
				if (count < ENDPOINT_STRLEN_MAX) {
					char buf[ENDPOINT_STRLEN_MAX];

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));
					_to_port = remove_port(buf);
					if (!ipaddr_aton(buf, &_to_addr))
						break;

					err_t err = udp_connect(_pcb, &_to_addr, _to_port);
					if (err != ERR_OK) {
						Genode::error("lwIP: failed to connect UDP socket, error ", (int)-err);
						return Write_result::WRITE_ERR_IO;
					}

					out_count = count;
					return Write_result::WRITE_OK;
				}
				break;
			}

			default: break;
			}

			return Write_result::WRITE_ERR_INVALID;
		}

		Sync_result complete_sync() override { return Sync_result::SYNC_OK; };
};


/*********
 ** TCP **
 *********/

class Lwip::Tcp_socket_dir final :
	public Socket_dir,
	public Tcp_socket_dir_list::Element
{
	private:

		Tcp_proto_dir       &_proto_dir;
		Genode::Allocator   &_alloc;
		Genode::Entrypoint  &_ep;
		Tcp_socket_dir_list  _pending;
		tcp_pcb             *_pcb;

		/* queue of received data */
		pbuf *_recv_pbuf = nullptr;
		u16_t _recv_off  = 0;

	public:

		State state = NEW;

		/* file_size pending_ack = 0; */

		Tcp_socket_dir(unsigned num, Tcp_proto_dir &proto_dir,
		               Genode::Allocator &alloc,
		               Vfs::Io_response_handler &io_handler,
		               Genode::Entrypoint &ep)
		: Socket_dir(num, alloc, io_handler), _proto_dir(proto_dir),
		  _alloc(alloc), _ep(ep), _pcb(tcp_new())
		{
			/* 'this' will be the argument to LwIP callbacks */
			tcp_arg(_pcb, this);

			tcp_recv(_pcb, tcp_recv_callback);
			/* Disabled, do not track acknowledgements */
			/* tcp_sent(_pcb, tcp_sent_callback); */
			tcp_err(_pcb, tcp_err_callback);
		}

		Tcp_socket_dir(unsigned num, Tcp_proto_dir &proto_dir,
		               Genode::Allocator &alloc,
		               Vfs::Io_response_handler &io_handler,
				   Genode::Entrypoint &ep,
		               tcp_pcb *pcb)
		: Socket_dir(num, alloc, io_handler), _proto_dir(proto_dir),
		  _alloc(alloc), _ep(ep), _pcb(pcb)
		{
			/* 'this' will be the argument to LwIP callbacks */
			tcp_arg(_pcb, this);

			tcp_recv(_pcb, tcp_recv_callback);
			/* Disabled, do not track acknowledgements */
			/* tcp_sent(_pcb, tcp_sent_callback); */
			tcp_err(_pcb, tcp_err_callback);

			/* the PCB comes connected and ready to use */
			state = READY;
		}

		~Tcp_socket_dir()
		{
			for (Tcp_socket_dir *p = _pending.first(); p; p->next()) {
				destroy(_alloc, p);
			}

			tcp_arg(_pcb, NULL);

			if (state != CLOSED && _pcb != NULL) {
				tcp_close(_pcb);
			}
		}

		/**
		 * Accept new connection from callback
		 */
		err_t accept(struct tcp_pcb *newpcb, err_t err)
		{
			try {
				/*
				 * use the equidistribution RNG to hide the socket count,
				 * see src/lib/lwip/platform/rand.cc
				 */
				unsigned id = LWIP_RAND();

				Tcp_socket_dir *new_socket = new (_alloc)
					Tcp_socket_dir(id, _proto_dir, _alloc, io_handler, _ep, newpcb);

				_pending.insert(new_socket);

				tcp_backlog_delayed(_pcb);
			} catch (...) {
				Genode::error("LwIP: unhandled error, dropping incoming connection");
				tcp_abort(newpcb);
				return ERR_ABRT;
			}

			handle_io(accept_handles);

			return ERR_OK;
		}

		/**
		 * chain a buffer to the queue
		 */
		void recv(struct pbuf *buf)
		{
			if (_recv_pbuf && buf) {
				pbuf_cat(_recv_pbuf, buf);
			} else {
				_recv_pbuf = buf;
			}
		}

		/**
		 * Close the connection by error
		 *
		 * Triggered by error callback, usually
		 * just by an aborted connection.
		 */
		void error()
		{
			state = CLOSED;
			/* the PCB is expired now */
			_pcb = NULL;

			/* churn the application */
			handle_io(remote_handles);
			handle_io(data_handles);
		}

		/**
		 * Close the connection
		 *
		 * Can be triggered by remote shutdown via callback
		 */
		void shutdown()
		{
			state = CLOSING;
			if (_recv_pbuf)
				return;

			tcp_close(_pcb);
			tcp_arg(_pcb, NULL);
			state = CLOSED;
			/* lwIP may reuse the PCB memory for the next connection */
			_pcb = NULL;
		}

		/**************************
		 ** Socket_dir interface **
		 **************************/

		bool read_ready(Lwip_handle &handle) override
		{
			typedef Lwip_handle::Type Type;
			switch (handle.type) {
			case Type::DATA:
				switch (state) {
				case READY:
					return _recv_pbuf != NULL;
				case CLOSING:
				case CLOSED:
					/* time for the application to find out */
					return true;
				default: break;
				}
				break;

			case Type::ACCEPT:
				return _pending.first() != nullptr;

			case Type::BIND:
				return state != NEW;

			case Type::REMOTE:
				switch (state) {
				case NEW:
				case BOUND:
				case LISTEN:
					break;
				default:
					return true;
				}
				break;

			case Type::CONNECT:
				return !ip_addr_isany(&_pcb->remote_ip);

			case Type::LOCAL:
				return true;
			default: break;
			}

			return false;
		}

		Read_result read(Lwip_handle &handle,
		                 char *dst, file_size count,
		                 file_size &out_count) override
		{
			if (_pcb == NULL)
				return Read_result::READ_OK;

			typedef Lwip_handle::Type Type;

			switch(handle.type) {

			case Type::DATA:
				if (state == READY || state == CLOSING) {
					if (_recv_pbuf == nullptr) {
						return Read_result::READ_QUEUED;
					}

					u16_t const ucount = count;
					u16_t const n = pbuf_copy_partial(_recv_pbuf, dst, ucount, _recv_off);
					_recv_off += n;
					{
						u16_t new_off;
						pbuf *new_head = pbuf_skip(_recv_pbuf, _recv_off, &new_off);
						if (new_head != NULL && new_head != _recv_pbuf) {
							/* move down the buffer and deref the head */
							pbuf_ref(new_head);
							pbuf_realloc(new_head, _recv_pbuf->tot_len+_recv_off);
							pbuf_free(_recv_pbuf);
						}

						if (!new_head)
							pbuf_free(_recv_pbuf);
						_recv_pbuf = new_head;
						_recv_off = new_off;
					}

					/* ACK the remote */
					tcp_recved(_pcb, n);

					if (state == CLOSING)
						shutdown();

					out_count = n;
					return Read_result::READ_OK;
				} else if (state == CLOSED) {
					return Read_result::READ_OK;
				}
				break;

			case Type::REMOTE:
				if (state == READY) {
					if (count < ENDPOINT_STRLEN_MAX)
						return Read_result::READ_ERR_INVALID;
					char const *ip_str = ipaddr_ntoa(&_pcb->remote_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(dst, count, "%s:%d\n",
					                             ip_str, _pcb->remote_port);
					return Read_result::READ_OK;
				} else if (state == CLOSED) {
					return Read_result::READ_OK;
				}
				break;

			case Type::ACCEPT:
				if (state == LISTEN) {
					Tcp_socket_dir *new_sock = _pending.first();
					if (!new_sock)
						return Read_result::READ_QUEUED;

					/* walk to the end of the linked list */
					while (new_sock->next())
						new_sock = new_sock->next();

					Name const &new_id = new_sock->name();
					if ((new_id.length()+5) <= count) {
						_pending.remove(new_sock);
						_proto_dir.adopt_socket(*new_sock);
						tcp_backlog_accepted(_pcb);
						out_count = Genode::snprintf(
							dst, count, "tcp/%s\n", new_id.string());
						return Read_result::READ_OK;
					} else {
						Genode::error("VFS LwIP: cannot read a ",
						               (new_id.length()+5),
						               "byte accept path into a ",
						               count, " byte buffer");
						return Read_result::READ_ERR_INVALID;
					}
				}
				break;

			case Type::LOCAL:
			case Type::BIND:
				if (state != CLOSED) {
					if (count < ENDPOINT_STRLEN_MAX)
						return Read_result::READ_ERR_INVALID;
					char const *ip_str = ipaddr_ntoa(&_pcb->local_ip);
					/* TODO: [IPv6]:port */
					out_count = Genode::snprintf(
						dst, count, "%s:%d\n", ip_str, _pcb->local_port);
					return Read_result::READ_OK;
				}
				break;

			case Type::CONNECT:
			case Type::LISTEN:
			case Type::INVALID: break;
			}

			return Read_result::READ_ERR_INVALID;
		}

		Write_result write(Lwip_handle &handle,
		                   char const *src, file_size count,
		                   file_size &out_count) override
		{
			if (_pcb == NULL) {
				/* socket is closed */
				return Write_result::WRITE_ERR_INVALID;
			}

			typedef Lwip_handle::Type Type;

			switch(handle.type) {
			case Type::DATA:
				if (state == READY) {
					file_size out = 0;
					while (count) {
						/* check if the send buffer is exhausted */
						if (tcp_sndbuf(_pcb) == 0) {
							Genode::warning("TCP send buffer congested");
							out_count = out;
							return out
								? Write_result::WRITE_OK
								: Write_result::WRITE_ERR_WOULD_BLOCK;
						}

						u16_t n = min(count, tcp_sndbuf(_pcb));
						/* how much can we queue right now? */

						count -= n;
						/* write to outgoing TCP buffer */
						err_t err = tcp_write(
							_pcb, src, n, TCP_WRITE_FLAG_COPY);
						if (err == ERR_OK)
							/* flush the buffer */
							err = tcp_output(_pcb);
						if (err != ERR_OK) {
							Genode::error("lwIP: tcp_write failed, error ", (int)-err);
							return Write_result::WRITE_ERR_IO;
						}

						src += n;
						out += n;
						/* pending_ack += n; */
					}

					out_count = out;
					return Write_result::WRITE_OK;
				}
				break;

			case Type::ACCEPT: break;
			case Type::BIND:
				if ((state == NEW) && (count < ENDPOINT_STRLEN_MAX)) {
					char buf[ENDPOINT_STRLEN_MAX];
					ip_addr_t addr;
					u16_t port = 0;

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));

					port = remove_port(buf);
					if (!ipaddr_aton(buf, &addr))
						break;

					err_t err = tcp_bind(_pcb, &addr, port);
					if (err == ERR_OK) {
						state = BOUND;
						out_count = count;
						return Write_result::WRITE_OK;
					}
					return Write_result::WRITE_ERR_IO;
				}
				break;

			case Type::CONNECT:
				if (((state == NEW) || (state == BOUND)) && (count < ENDPOINT_STRLEN_MAX-1)) {
					char buf[ENDPOINT_STRLEN_MAX];
					ip_addr_t addr;
					u16_t port = 0;

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));
					port = remove_port(buf);
					if (!ipaddr_aton(buf, &addr))
						break;

					err_t err = tcp_connect(_pcb, &addr, port, tcp_connect_callback);
					if (err != ERR_OK) {
						Genode::error("lwIP: failed to connect TCP socket, error ", (int)-err);
						return Write_result::WRITE_ERR_IO;
					}
					state = CONNECT;
					out_count = count;
					return Write_result::WRITE_OK;
				}
				break;

			case Type::LISTEN:
				if ((state == BOUND) && (count < 7)) {
					unsigned long backlog = TCP_DEFAULT_LISTEN_BACKLOG;
					char buf[8];

					Genode::strncpy(buf, src, min(count+1, sizeof(buf)));
					Genode::ascii_to_unsigned(buf, backlog, 10);

					/* this replaces the PCB so set the callbacks again */
					_pcb = tcp_listen_with_backlog(_pcb, backlog);
					tcp_arg(_pcb, this);
					tcp_accept(_pcb, tcp_accept_callback);
					state = LISTEN;
					out_count = count;
					return Write_result::WRITE_OK;
				}
				break;

			case Type::LOCAL:
			case Type::REMOTE:
			case Type::INVALID: break;
			}

			return Write_result::WRITE_ERR_INVALID;
		}

		Sync_result complete_sync() override
		{
			/* sync will queue until the socket is connected and ready */
			return (state == CONNECT) ?
				Sync_result::SYNC_QUEUED : Sync_result::SYNC_OK;
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
	if (!arg) return;

	Lwip::Udp_socket_dir *socket_dir = static_cast<Lwip::Udp_socket_dir *>(arg);
	socket_dir->queue(addr, port, p);
}


static
err_t tcp_connect_callback(void *arg, struct tcp_pcb *pcb, err_t err)
{
	if (!arg) return ERR_ARG;

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	socket_dir->state = Lwip::Tcp_socket_dir::READY;

	socket_dir->handle_io(socket_dir->data_handles);
	return ERR_OK;
}


static
err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	if (!arg) return ERR_ARG;

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	return socket_dir->accept(newpcb, err);
};


static
err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	if (!arg) return ERR_ARG;

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	if (p == NULL) {
		socket_dir->shutdown();
	} else {
		socket_dir->recv(p);
	}
	socket_dir->handle_io(socket_dir->data_handles);
	return ERR_OK;
}


/*
static
err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	if (!arg) return ERR_ARG;

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	socket_dir->pending_ack -= len;
	socket_dir->handle_io(socket_dir->data_handles);
	return ERR_OK;
}
*/


static
void tcp_err_callback(void *arg, err_t err)
{
	if (!arg) return;

	Lwip::Tcp_socket_dir *socket_dir = static_cast<Lwip::Tcp_socket_dir *>(arg);
	socket_dir->error();
	/* the error is ERR_ABRT or ERR_RST, both end the session */
}

	}
}


/*********************
 ** VFS file-system **
 *********************/

class Lwip::File_system final : public Vfs::File_system
{
	private:

		struct Vfs_netif : Lwip::Nic_netif
		{
			Vfs::Io_response_handler &io_handler;

			/**
			 * Wake the application when the interface changes.
			 */
			void status_callback() override {
				io_handler.handle_io_response(nullptr); }

			Vfs_netif(Genode::Env &env,
			          Genode::Allocator &alloc,
			          Genode::Xml_node config,
			          Vfs::Io_response_handler &io)
			: Lwip::Nic_netif(env, alloc, config),
			  io_handler(io)
			{ }
		} _netif;

		Tcp_proto_dir _tcp_dir;
		Udp_proto_dir _udp_dir;

		Read_result _read(Vfs_handle *vfs_handle,
		                  char *dst, file_size count,
		                  file_size &out_count)
		{
			out_count = 0;

			if ((!vfs_handle) ||
			    ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_WRONLY)) {
				return Read_result::READ_ERR_INVALID;
			}

			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle)) {
				if (!handle->socket) /* if the socket was removed then it was closed */
					return Read_result::READ_ERR_INVALID;

				return handle->socket->read(*handle, dst, count, out_count);
			} else if (Lwip_new_handle *handle = dynamic_cast<Lwip_new_handle*>(vfs_handle)) {
				/* postpone new sockets until the interface is ready */
				if (!_netif.ready())
					return Read_result::READ_QUEUED;


				if (count < (Socket_dir::Name::capacity()+1)) {
					return Read_result::READ_ERR_INVALID;
				}

				try {
					/*
					 * pass the allocator on the 'new_socket' handle to
					 * keep per-socket allocation out of the main heap
					 */
					Socket_dir::Name const &id = handle->proto_dir.alloc_socket(handle->alloc());
					if (dynamic_cast<Tcp_proto_dir*>(&handle->proto_dir))
						out_count = Genode::snprintf(
							dst, count, "tcp/%s\n", id.string());
					else if (dynamic_cast<Udp_proto_dir*>(&handle->proto_dir))
						out_count = Genode::snprintf(
							dst, count, "udp/%s\n", id.string());
					return Read_result::READ_OK;
				} catch (...) {
					Genode::error("failed to allocate new LwIP socket");
					return Read_result::READ_ERR_IO;
				}
			}

			Genode::error("read failed");
			return Read_result::READ_ERR_INVALID;
		}

	public:

		File_system(Genode::Env       &env,
		            Genode::Allocator &alloc,
		            Genode::Xml_node   config,
		            Vfs::Io_response_handler &io_handler)
		:
			_netif(env, alloc, config, io_handler),
			_tcp_dir(alloc, io_handler, env.ep()),
			_udp_dir(alloc, io_handler, env.ep())
		{ }

		~File_system() { }

		void apply_config(Genode::Xml_node const &node) override {
			_netif.configure(node); }


		/***********************
		 ** Directory_service **
		 ***********************/

		char const *leaf_path(char const *path) override
		{
			if (*path == '/') ++path;
			if (Genode::strcmp(path, "tcp", 3) == 0)
				return _tcp_dir.leaf_path(path+3) ? path : nullptr;
			if (Genode::strcmp(path, "udp", 3) == 0)
				return _udp_dir.leaf_path(path+3) ? path : nullptr;
			return nullptr;
		}

		Stat_result stat(char const *path, Stat &st) override
		{
			if (*path == '/') ++path;
			st = Stat();
			st.device = (Genode::addr_t)this;

			Stat_result r = STAT_ERR_NO_PERM;

			if (Genode::strcmp(path, "tcp", 3) == 0)
				r = _tcp_dir.stat(path+3, st);
			else if (Genode::strcmp(path, "udp", 3) == 0)
				r = _udp_dir.stat(path+3, st);
			return r;
		}

		bool directory(char const *path) override
		{
			if (*path == '/') ++path;
			if (*path == '\0') return true;

			char const *subpath = path+3;
			if (Genode::strcmp(path, "tcp", 3) == 0)
				return (*subpath) ? (_tcp_dir.lookup(subpath+1) != nullptr) : true;
			if (Genode::strcmp(path, "udp", 3) == 0)
				return (*subpath) ? (_udp_dir.lookup(subpath+1) != nullptr) : true;
			return false;
		}

		Open_result open(char const  *path,
		                 unsigned     mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (*path == '/') ++path;

			if (mode & OPEN_MODE_CREATE) return OPEN_ERR_NO_PERM;

			if (Genode::strcmp(path, "tcp", 3) == 0)
				return _tcp_dir.open(*this, path+3, mode, out_handle, alloc);
			if (Genode::strcmp(path, "udp", 3) == 0)
				return _udp_dir.open(*this, path+3, mode, out_handle, alloc);
			return OPEN_ERR_UNACCESSIBLE;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			if (Lwip_new_handle *handle = dynamic_cast<Lwip_new_handle*>(vfs_handle)) {
				destroy(vfs_handle->alloc(), handle);
			} else if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle)) {
				if (handle->socket)
					handle->socket->close(handle);
				destroy(vfs_handle->alloc(), handle);
			}
		}

		Unlink_result unlink(char const *path) override
		{
			if (*path == '/') ++path;
			if (Genode::strcmp(path, "tcp", 3) == 0)
				return _tcp_dir.unlink(path+3);
			if (Genode::strcmp(path, "udp", 3) == 0)
				return _udp_dir.unlink(path+3);
			return UNLINK_ERR_NO_ENTRY;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *vfs_handle,
		                   char const *src, file_size count,
		                   file_size &out_count) override
		{
			out_count = 0;

			if ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_RDONLY)
				return Write_result::WRITE_ERR_INVALID;

			/*
			if ((!vfs_handle) ||
			    ((vfs_handle->status_flags() & OPEN_MODE_ACCMODE) == OPEN_MODE_RDONLY)) {
				return Write_result::WRITE_ERR_INVALID;
				return;
			}
			 */

			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle)) {
				if (handle->socket) {
					return handle->socket->write(*handle, src, count, out_count);
				} else {
					Genode::error(*handle, " has no socket");
				}
			} else {
				Genode::error("handle not casted to Lwip_handle");
			}
			Genode::error("write failed");
			return Write_result::WRITE_ERR_INVALID;
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                                  char *dst, file_size count,
		                                  file_size &out_count) override
		{
			return _read(vfs_handle, dst, count, out_count);
		}

		bool read_ready(Vfs_handle *vfs_handle) override
		{
			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle)) {
				if (handle->socket)
					return handle->socket->read_ready(*handle);
			}

			/*
			 * in this case the polled file is a 'new_socket'
			 * or a file with no associated socket
			 */
			return true;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			if (Lwip_handle *handle = dynamic_cast<Lwip_handle*>(vfs_handle)) {
				if (handle->socket) {
					return true;
				}
			}
			return false;
		}

		bool check_unblock(Vfs_handle *vfs_handle,
		                           bool rd, bool wr, bool ex)
		{
			Genode::error("VFS lwIP: ",__func__," not implemented");
			return true;
		}

		Sync_result complete_sync(Vfs_handle *vfs_handle) override
		{
			Lwip_handle *h = dynamic_cast<Lwip_handle*>(vfs_handle);
			if (h) {
				if (h->socket) {
					return h->socket->complete_sync();
				} else {
					return SYNC_QUEUED;
				}
			}
			return SYNC_OK;
		}

		/***********************
		 ** File system stubs **
		 ***********************/

		Rename_result rename(char const *from, char const *to) override {
			return RENAME_ERR_NO_PERM; }

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

		char const *type() override { return "lwip"; }
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Genode::Constructible<Timer::Connection> timer;

		Vfs::File_system *create(Genode::Env &env,
		                         Genode::Allocator &alloc,
		                         Genode::Xml_node config,
		                         Vfs::Io_response_handler &io_handler) override
		{
			if (!timer.constructed()) {
				timer.construct(env, "vfs_lwip");
				Lwip::genode_init(alloc, *timer);
			}

			return new (alloc) Lwip::File_system(env, alloc, config, io_handler);
		}
	};

	static Factory f;
	return &f;
}
