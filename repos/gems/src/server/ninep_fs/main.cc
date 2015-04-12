/*
 * \brief  9P client, File_system server
 * \author Emery Hemingway
 * \date   2015-04-12
 *
 * TODO:
 * Cache fids to prevent execessive
 * walking around and clunking about.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes. */
#include <file_system/node_handle_registry.h>
#include <file_system/util.h>
#include <file_system_session/rpc_object.h>
#include <timer_session/connection.h>
#include <base/printf.h>
#include <base/heap.h>
#include <root/component.h>
#include <os/config.h>
#include <os/session_policy.h>
#include <os/server.h>

/* Local includes */
#include "directory.h"
#include "session.h"


namespace File_system {

	using namespace Genode;

	struct Main;
	struct Session_component;
	struct Root;
}

using namespace Plan9;

class File_system::Session_component : public Session_rpc_object
{
	private:

		Node_handle_registry _handle_registry;
		Allocator           &_md_alloc;
		bool                 _writeable;
		Fid           _rootfid;
		Attachment    _attach;

		Signal_rpc_member<Session_component> _process_packet_dispatcher;

		/******************************
		 ** Packet-stream processing **
		 ******************************/

		/**
		 * Perform packet operation
		 *
		 * \return true on success, false on failure
		 */
		void _process_packet_op(Packet_descriptor &packet, Node &node)
		{
			using namespace Genode;

			void     * const content = tx_sink()->packet_content(packet);
			Genode::size_t     const length  = packet.length();
			seek_off_t const offset  = packet.position();

			if (!content || (packet.length() > packet.size())) {
				packet.succeeded(false);
				return;
			}

			/* resulting length */
			Genode::size_t res_length = 0;

			switch (packet.operation()) {

			case Packet_descriptor::READ:
				res_length = node.read((char *)content, length, offset);
				break;

			case Packet_descriptor::WRITE:
				res_length = node.write((char const *)content, length, offset);
				break;
			}

			packet.length(res_length);
			packet.succeeded(res_length > 0);
		}

		void _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

			/* assume failure by default */
			packet.succeeded(false);

			try {
				Node *node = _handle_registry.lookup_and_lock(packet.handle());
				Node_lock_guard guard(node);

				_process_packet_op(packet, *node);
			}
			catch (Invalid_handle)     { PERR("Invalid_handle");     }
			catch (Size_limit_reached) { PERR("Size_limit_reached"); }

			/*
			 * The 'acknowledge_packet' function cannot block because we
			 * checked for 'ready_to_ack' in '_process_packets'.
			 */
			tx_sink()->acknowledge_packet(packet);
		}

		/**
		 * Called by signal dispatcher, executed in the context of the main
		 * thread (not serialized with the RPC functions)
		 */
		void _process_packets(unsigned)
		{
			while (tx_sink()->packet_avail()) {

				/*
				 * Make sure that the '_process_packet' function does not
				 * block.
				 *
				 * If the acknowledgement queue is full, we defer packet
				 * processing until the client processed pending
				 * acknowledgements and thereby emitted a ready-to-ack
				 * signal. Otherwise, the call of 'acknowledge_packet()'
				 * in '_process_packet' would infinitely block the context
				 * of the main thread. The main thread is however needed
				 * for receiving any subsequent 'ready-to-ack' signals.
				 */
				if (!tx_sink()->ready_to_ack())
					return;

				_process_packet();
			}
		}

		/**
		 * Check if string represents a valid path (most start with '/')
		 */
		static void _assert_valid_path(char const *path)
		{
			if (!path || path[0] != '/') {
				PWRN("malformed path `%s'", path);
				throw Lookup_failed();
			}
		}

		/**
		 * Walk a path rooted at '/'.
		 */
		Fid walk_path(char const *absolute)
		{
			Fcall tx, rx;
			tx.fid = _rootfid;
			tx.nwname = 0;

			char path[MAX_PATH_LEN];
			int i = 0;

			strncpy(path, absolute, sizeof(path));

			int elements = 0;
			int up_count = 0;
			while(path[++i]) {
				if (path[i] == '/') {
					++elements;
					if (path[++i] == '.' && path[i+1] == '.')
						 if ((++up_count)*2 > elements)
							/* There might be something at /.. but you'll not get there. */
							throw Permission_denied();
				}
			}

			/* Skip the leading '/' */
			i = 1;
			while (path[i]) {
				tx.wname[tx.nwname++] = &path[i];

				while (path[++i])
					if (path[i] == '/') {
						path[i++] = 0;
						break;
					}

				if (tx.nwname == MAXWELEM) {
					tx.newfid = unique_fid();
					_attach.transact(Twalk, &tx, &rx);
					if (tx.fid != _rootfid)
						_attach.clunk(tx.fid);
					tx.fid = tx.newfid;
					tx.newfid = unique_fid();
					if (rx.nwqid < tx.nwname)
						throw Lookup_failed();
					tx.nwname = 0;
				}
			}
			if (tx.nwname) {
				tx.newfid = unique_fid();
				_attach.transact(Twalk, &tx, &rx);
				if (tx.fid != _rootfid)
					_attach.clunk(tx.fid);
				if (rx.nwqid < tx.nwname)
						throw Lookup_failed();
			} else
				return _attach.duplicate(_rootfid);
			return tx.newfid;
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::size_t      tx_buf_size,
		                  Server::Entrypoint &ep,
		                  Socket      &sock,
		                  Allocator          &md_alloc,
		                  char const *uname, char const *aname,
		                  char const *base_dir, bool writeable)
		:
			Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
			_md_alloc(md_alloc),
			_writeable(writeable),
			_rootfid(unique_fid()),
			_attach(sock, _rootfid, uname, aname),
			_process_packet_dispatcher(ep, *this, &Session_component::_process_packets)
		{
			if (strcmp(base_dir, "/")) {
				/* Walk to the new root and replace _rootfid. */
				try {
					Fid basefid = walk_path(base_dir);
					_attach.clunk(_rootfid);
					_rootfid = basefid;
				} catch (...) {
					_attach.clunk(_rootfid);
					PERR("session root '%s' is unavailable", base_dir);
					throw File_system::Exception();
				}
			}

			/*
			 * Register `_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			Dataspace_capability ds = tx_sink()->dataspace();
			env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
			_attach.clunk(_rootfid);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		Dir_handle dir(File_system::Path const &path, bool create)
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			if (!_writeable && create)
				throw Permission_denied();

			if (!path.is_valid_string())
				throw Name_too_long();

			if (create) {
				Genode::Path<MAX_PATH_LEN> dir_path(path_str);
				char const *name_str = basename(path_str);

				if (dir_path.has_single_element()) {
					_attach.create(_rootfid, name_str,
					               DMDIR|0xFFFF, OREAD);
				} else {
					dir_path.strip_last_element();
					Fid parent_fid = walk_path(dir_path.base());
					_attach.create(parent_fid, name_str,
					               DMDIR|0xFFFF, OREAD);
					_attach.clunk(parent_fid);
				}
			}

			Directory *dir = new (&_md_alloc)
				Directory(_attach, walk_path(path_str));
			return _handle_registry.alloc(dir);
		}

		File_handle file(Dir_handle dir_handle, Name const &name,
		                 Mode mode, bool create)
		{
			char name_str[name.size()];
			strncpy(name_str, name.string(), sizeof(name_str));
			if (!valid_name(name_str))
				throw Invalid_name();

			if (!_writeable)
				if (create || (mode != STAT_ONLY && mode != READ_ONLY))
					throw Permission_denied();

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			if (create)
				_attach.create(dir->fid(), name_str,
				                0xFFFF, convert_mode(mode));

			File *file = new (&_md_alloc)
				File(_attach, dir->walk_name(name_str), mode);
			return _handle_registry.alloc(file);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
		{;
			/* 9P does not support symlinks. */
			throw Permission_denied();
		}

		Node_handle node(File_system::Path const &path)
		{
			char const *path_str = path.string();
			_assert_valid_path(path_str);

			Node *node = new (&_md_alloc)
				Node(_attach, walk_path(path_str));
			return _handle_registry.alloc(node);
		}

		void close(Node_handle handle)
		{
			try {
				Node *node = _handle_registry.lookup(handle);
				_handle_registry.free(handle);
				destroy(&_md_alloc, node);
				/* The node destructor clunks. */
			} catch (Invalid_handle) { }
		}

		Status status(Node_handle node_handle)
		{
			char buf[1024];
			Fcall tx, rx;
			Status stat;

			Node *node = _handle_registry.lookup_and_lock(node_handle);
			Node_lock_guard guard(node);

			tx.fid = node->fid();
			rx.stat = buf;
			_attach.transact(Tstat, &tx, &rx);

			int bi = 2+2+4+1+4;
			get8(rx.stat, bi, stat.inode);

			/* The high bit of the qid type byte denotes a directory. */
			if (rx.stat[2+2+4] & 0x80) {
				stat.size = _attach.num_entries(node->fid())
					* sizeof(Directory_entry);
				stat.mode = Status::MODE_DIRECTORY;
			} else {
				stat.mode = Status::MODE_FILE;
				bi = 2+2+4+13+4+4+4;
				get8(rx.stat, bi, stat.size);
			}
			return stat;
		}

		void control(Node_handle, Control) {
			PWRN("%s not implemented", __func__); }

		void unlink(Dir_handle dir_handle, Name const &name)
		{
			Fcall tx;

			char name_str[name.size()];
			strncpy(name_str, name.string(), sizeof(name_str));

			if (!valid_name(name_str)) throw Invalid_name();

			if (!_writeable) throw Permission_denied();

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			tx.fid = dir->walk_name(name_str);
			_attach.transact(Tremove, &tx, 0);
		}

		void truncate(File_handle file_handle, file_size_t size)
		{
			uint8_t buf[NULL_STAT_LEN];
			Fcall tx;

			if (!_writeable) throw Permission_denied();

			File *file = _handle_registry.lookup_and_lock(file_handle);
			Node_lock_guard file_guard(file);

			tx.fid = file->fid();
			tx.nstat = NULL_STAT_LEN;
			tx.stat = (char*)buf;

			zero_stat(buf);
			int bi = 2+2+4+13+4+4+4;
			put8(buf, bi, size);

			_attach.transact(Twstat, &tx);
		}

		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle   to_dir_handle, Name const   &to_name)
		{
			if (_writeable)
				throw Permission_denied();

			if (!valid_name(from_name.string()))
				throw Lookup_failed();

			if (!valid_name(to_name.string()))
				throw Invalid_name();

			Directory *from_dir = _handle_registry.lookup_and_lock(from_dir_handle);
			Node_lock_guard from_dir_guard(from_dir);

			char from_str[from_name.size()];
			strncpy(from_str, from_name.string(), sizeof(from_str));

			Node node(_attach, from_dir->walk_name(from_str));

			if (_handle_registry.refer_to_same_node(from_dir_handle, to_dir_handle)) {
				node.rename(to_name);
			} else {
				PWRN("node moving not implemented");
				/*
				 * TODO:
				 * 9P doesn't have a move verb,
				 * so this would be a deep copy.
				 */
				throw Permission_denied();
			}

			from_dir->mark_as_updated();
			from_dir->notify_listeners();
		}

		void sigh(Node_handle node_handle, Signal_context_capability sigh) {
			_handle_registry.sigh(node_handle, sigh); }

		void sync() { PWRN("9P does not support sync()"); }
};

class File_system::Root : public Root_component<Session_component>
{
	private:

		Server::Entrypoint &_ep;
		Socket              _sock;

	protected:

		Session_component *_create_session(const char *args)
		{
			/*
			 * Determine client_specific policy defined implicitly by
			 * the client's label.
			 */
			char const *root_dir  = ".";
			bool writeable = false;

			char uname[MAX_NAME_LEN];
			char aname[MAX_NAME_LEN];
			char  root[MAX_NAME_LEN];
			root[0] = '\0';

			try {
				Session_label label(args);
				Session_policy policy(label);

				/*
				 * Determine the directory that is used as the root directory of
				 * the session.
				 */
				try {
					policy.attribute("user").value(uname, sizeof(uname));
				} catch (Xml_node::Nonexistent_attribute) {
					strncpy(uname, "glenda", sizeof(uname));
				}
				try {
					policy.attribute("access").value(aname, sizeof(aname));
				} catch (Xml_node::Nonexistent_attribute) {
					aname[0] = '\0';
				}
				try {
					policy.attribute("root").value(root, sizeof(root));
					/*
					 * Make sure the root path is specified with a
					 * leading path delimiter. For performing the
					 * lookup, we skip the first character.
					 */
					if (root[0] != '/') {
						PERR("Session root directory `%s' is not absolute", root);
						throw Root::Unavailable();
					}

					root_dir = root;
				} catch (Xml_node::Nonexistent_attribute) {
					PERR("Missing `root' attribute in policy definition");
					throw Root::Unavailable();
				}

				/*
				 * Determine if write access is permitted for the session.
				 */
				try {
					writeable = policy.attribute("writeable").has_value("yes");
				} catch (Xml_node::Nonexistent_attribute) {}

			} catch (Session_policy::No_policy_defined) {
				PERR("Invalid session request, no matching policy");
				throw Root::Unavailable();
			}

			File_system::size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			File_system::size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			File_system::size_t session_size = sizeof(Session_component) + tx_buf_size;
			if (max((Genode::size_t)4096, session_size) > ram_quota) {
				PERR("insufficient `ram_quota', got %ld, need %ld",
				     ram_quota, session_size);
				throw Root::Quota_exceeded();
			}
			return new (md_alloc())
				Session_component(tx_buf_size, _ep,
				                  _sock,      *md_alloc(),
				                  uname,       aname,
				                  root_dir,    writeable);
		}

	public:

		/**
		 * Constructor
		 * \param ep       entrypoint
		 * \param md_alloc meta-data allocator
		 * \param root_dir root-directory
		 */
		Root(Server::Entrypoint &ep, Allocator &md_alloc)
		:
			Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ep(ep)
		{ }

		void connect()
		{
			do {
				char addr_remote[INET_ADDRSTRLEN];
				unsigned long port = DEFAULT_PORT;
				unsigned long message_size = MAX_MESSAGE_SIZE;

				/* Read the config. */
				Xml_node xml_node = config()->xml_node();
				try {
					xml_node.attribute("addr").value(addr_remote, sizeof(addr_remote));
				} catch (Xml_node::Nonexistent_attribute &e) {
					PERR("9P server address not specified with 'addr'");
					throw e;
				}

				try { xml_node.attribute("port").value(&port); } catch (...) { }
				try { xml_node.attribute("message_size").value(&message_size); } catch (...) { }

				PLOG("connecting to %s", addr_remote);
				try {
					_sock.connect(addr_remote, port, message_size);
				} catch (Socket::Permanent_failure &e) {
					PERR("Failed to connect to 9P server, exiting");
					throw e;
				} catch (Socket::Temporary_failure) {
					Timer::Connection().usleep(8000000);
				}
			} while (!_sock.connected());
		}
};


struct File_system::Main
{
	Server::Entrypoint &ep;

	/*
	 * Initialize root interface.
	 */
	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root fs_root{ep, sliced_heap};

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		fs_root.connect();
		env()->parent()->announce(ep.manage(fs_root));
	}
};


/**********************
 ** Server framework **
 **********************/

char const *   Server::name()                            { return "9p_client_ep"; }
Genode::size_t Server::stack_size()                      { return 2048 * sizeof(long); }
void           Server::construct(Server::Entrypoint &ep) { static File_system::Main inst(ep); }
