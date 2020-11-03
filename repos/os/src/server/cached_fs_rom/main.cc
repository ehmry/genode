/*
 * \brief  Component that caches files to be served as ROMs
 * \author Emery Hemingway
 * \date   2018-04-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/path.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <rom_session/rom_session.h>
#include <region_map/client.h>
#include <rm_session/connection.h>
#include <base/attached_ram_dataspace.h>
#include <base/session_label.h>
#include <base/heap.h>
#include <base/component.h>

/* local session-requests utility */
#include "session_requests.h"


namespace Cached_fs_rom {

	using namespace Genode;

	struct Transfer;
	typedef Genode::Id_space<Transfer> Transfer_space;

	struct Cached_rom;
	typedef Genode::Id_space<Cached_rom> Cache_space;

	class Session_component;
	typedef Genode::Id_space<Session_component> Session_space;

	struct Main;

	typedef Genode::Path<File_system::MAX_PATH_LEN> Path;
	typedef File_system::Session_client::Tx::Source Tx_source;
	typedef File_system::Session::Tx::Source::Packet_alloc_failed Packet_alloc_failed;
}


/**
 * State of symlink resolution and file reading
 */
struct Cached_fs_rom::Transfer final
{
	Transfer_space          &_transfers;
	File_system::Session    &_fs;

	Path const first_path;
	Path       final_path = first_path;

	Reconstructible<Transfer_space::Element> _transfer_elem {
		*this, _transfers,
		Transfer_space::Id{_fs.node(first_path.string()).value}
	};

	File_system::Packet_descriptor _raw_pkt = _alloc_packet();
	File_system::Packet_guard      _packet_guard { *_fs.tx(), _raw_pkt };

	Attached_ram_dataspace ram_ds;
	File_system::Status    status { };

	/* Id of the originating session request */
	Parent::Server::Id const request_id;

	File_system::seek_off_t  _seek = 0;

	/**
	 * Allocate space in the File_system packet buffer
	 *
	 * \throw  Packet_alloc_failed
	 */
	File_system::Packet_descriptor _alloc_packet()
	{
		if (!_fs.tx()->ready_to_submit())
			throw Packet_alloc_failed();

		size_t chunk_size = max(
			size_t(File_system::MAX_PATH_LEN),
			_fs.tx()->bulk_buffer_size()/4);
		return _fs.tx()->alloc_packet(chunk_size);
	}

	File_system::Node_handle _handle() const {
		return File_system::Node_handle{ _transfer_elem->id().value }; }

	void _replace_handle(File_system::Node_handle new_handle)
	{
		_fs.close(_handle());
		_transfer_elem.destruct();
		_transfer_elem.construct(
			*this, _transfers, Transfer_space::Id{new_handle.value});
	}

	void _resolve()
	{
		using namespace File_system;
		status = _fs.status(_handle());

		if (status.directory()) {
			error("cannot serve directory as ROM, \"", final_path, "\"");
			throw Service_denied();
		}

		Path dir_path(final_path);
		dir_path.strip_last_element();
		Dir_handle parent_handle = _fs.dir(dir_path.base(), false);
		Handle_guard parent_guard(_fs, parent_handle);

		if (status.symlink()) {
			Symlink_handle link_handle = _fs.symlink(
				parent_handle, final_path.last_element(), false);
			_replace_handle(link_handle);
		} else {
			File_handle file_handle = _fs.file(
				parent_handle, final_path.last_element(), READ_ONLY, false);
			_replace_handle(file_handle);
		}
	}

	Transfer(Genode::Env          &env,
	         File_system::Session &fs,
	         Transfer_space       &space,
	         Path                  path,
	         Parent::Server::Id   &pid)
	: _transfers(space)
	, _fs(fs)
	, first_path(path)
	, ram_ds(env.ram(), env.rm(), 0)
	, request_id(pid)
	{
		_resolve();
	}

	~Transfer() { _fs.close(_handle()); }

	bool matches(Path const &path) const {
		return (first_path == path || final_path == path); }

	bool completed() const {
		return (!status.symlink() && _seek >= status.size); }

	/**
	 * Called from the packet signal handler.
	 */
	void process_packet(File_system::Packet_descriptor const packet)
	{
		if (status.symlink()) {
			size_t const n = packet.length();
			if (n >= Path::capacity()) {
				error("erronous symlink read");
				throw File_system::Lookup_failed();
			}

			char buf[Path::capacity()];
			copy_cstring(buf, _fs.tx()->packet_content(packet), n);
			if (*buf == '/')
				final_path = Path(buf);
			else
				final_path.append_element(buf);

			_replace_handle(_fs.node(final_path.string()));
			_resolve();

		} else {
			auto const pkt_seek = packet.position();

			if (pkt_seek > _seek || _seek >= status.size) {
				error("unexpected packet seek position for ", final_path);
				error("packet seek is ", packet.position(), ", file seek is ", _seek, ", file size is ", status.size);
				_seek = status.size;
			} else {
				size_t const n = min(packet.length(), status.size - pkt_seek);
				memcpy(ram_ds.local_addr<char>()+pkt_seek,
				       _fs.tx()->packet_content(packet), n);
				_seek = pkt_seek+n;
			}
		}
	}

	void submit_next_packet()
	{
		File_system::Packet_descriptor packet(
			_raw_pkt, _handle(),
			File_system::Packet_descriptor::READ,
			min(_raw_pkt.size(), status.size-_seek), _seek);
		_fs.tx()->submit_packet(packet);
	}

};


/**
 * A dataspace corresponding to a file-system path
 */
struct Cached_fs_rom::Cached_rom final
{
	Cached_rom(Cached_rom const &);
	Cached_rom &operator = (Cached_rom const &);

	Genode::Env   &env;
	Rm_connection &rm_connection;

	/**
	 * Backing RAM dataspace
	 */
	Attached_ram_dataspace ram_ds { env.pd(), env.rm(), 0 };

	/**
	 * Read-only region map exposed as ROM module to the client
	 */
	Region_map_client      rm_client;
	Region_map::Local_addr rm_attachment { };
	Dataspace_capability   rm_ds { };

	Path const path;

	Cache_space::Element cache_elem;

	/**
	 * Reference count of cache entry
	 */
	int _ref_count = 0;

	Cached_rom(Genode::Env            &env,
	           Rm_connection          &rm,
	           Cache_space            &cache_space,
	           Transfer               &transfer)
	: env(env)
	, rm_connection(rm)
	, rm_client(rm.create(transfer.ram_ds.size()))
	, path(transfer.final_path)
	, cache_elem(*this, cache_space)
	{
		/* move dataspace from the transfer object */
		transfer.ram_ds.swap(ram_ds);

		/* attach dataspace read-only into region map */
		enum { OFFSET = 0, LOCAL_ADDR = false, EXEC = true, WRITE = false };
		rm_attachment = rm_client.attach(
			ram_ds.cap(), ram_ds.size(), OFFSET,
			LOCAL_ADDR, (addr_t)~0, EXEC, WRITE);
		rm_ds = rm_client.dataspace();
	}

	~Cached_rom()
	{
		if (rm_attachment)
			rm_client.detach(rm_attachment);
	}

	bool unused() const { return (_ref_count < 1); }

	/**
	 * Return dataspace with content of file
	 */
	Rom_dataspace_capability dataspace() const {
		return static_cap_cast<Rom_dataspace>(rm_ds); }

	/**
	 * Guard for maintaining reference count
	 */
	struct Guard
	{
		Cached_rom &_rom;

		Guard(Cached_rom &rom) : _rom(rom) {
			++_rom._ref_count; }
		~Guard() {
			--_rom._ref_count; };
	};
};


class Cached_fs_rom::Session_component final : public  Rpc_object<Rom_session>
{
	private:

		Cached_rom             &_cached_rom;
		Cached_rom::Guard       _cache_guard { _cached_rom };
		Session_space::Element  _sessions_elem;

	public:

		Session_component(Cached_rom &cached_rom,
		                  Session_space &space, Session_space::Id id)
		:
			_cached_rom(cached_rom),
			_sessions_elem(*this, space, id)
		{ }


		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override {
			return _cached_rom.dataspace(); }

		void sigh(Signal_context_capability) override { }

		bool update() override { return false; }
};


struct Cached_fs_rom::Main final : Genode::Session_request_handler
{
	Genode::Env &env;

	Rm_connection rm { env };

	Transfer_space transfers { };
	Cache_space    cache     { };
	Session_space  sessions  { };

	Heap heap { env.pd(), env.rm() };

	Allocator_avl           fs_tx_block_alloc { &heap };
	File_system::Connection fs { env, fs_tx_block_alloc };

	Session_requests_rom session_requests { env, *this };

	Io_signal_handler<Main> packet_handler {
		env.ep(), *this, &Main::handle_packets };

	/**
	 * Return true when a cache element is freed
	 */
	bool cache_evict()
	{
		Cached_rom *discard = nullptr;

		cache.for_each<Cached_rom&>([&] (Cached_rom &rom) {
			if (!discard && rom.unused()) discard = &rom; });

		if (discard)
			destroy(heap, discard);
		return (bool)discard;
	}

	/**
	 * Attempt a procedure with exception management
	 */
	template <typename PROC>
	void try_transfer(Path const &file_path, PROC const &proc)
	{
		using namespace File_system;
		try { proc(); return; }
		catch (Lookup_failed)     { error(file_path, " not found");          }
		catch (Invalid_handle)    { error(file_path, ": invalid handle");    }
		catch (Invalid_name)      { error(file_path, ": invalid name");      }
		catch (Permission_denied) { error(file_path, ": permission denied"); }
		catch (...)               { error(file_path, ": unhandled error");   }
		throw Service_denied();
	}

	void process_transfer(Transfer &transfer)
	{
		/* allocate a backing dataspace if the path is resolved */
		if (!transfer.ram_ds.size() && !transfer.status.symlink()) {
			size_t ds_size =
				align_addr(max(transfer.status.size, 1U), 12);

			while (env.pd().avail_ram().value < ds_size
			    || env.pd().avail_caps().value < 8) {
				 /* drop unused cache entries */
				 if (!cache_evict()) break;
			}

			transfer.ram_ds.realloc(&env.ram(), ds_size);
		}

		if (transfer.completed()) {
			new (heap) Cached_rom( env, rm, cache, transfer);
			destroy(heap, &transfer);
			session_requests.schedule();
		} else {
			transfer.submit_next_packet();
		}
	}

	/**
	 * Create new sessions
	 */
	void handle_session_create(Session_state::Name const &name,
	                           Parent::Server::Id pid,
	                           Session_state::Args const &args) override
	{
		if (name != "ROM") throw Service_denied();


		/************************************************
		 ** Enforce sufficient donation for RPC object **
		 ************************************************/

		size_t ram_quota =
			Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);
		size_t session_size =
			max((size_t)4096, sizeof(Session_component));

		if (ram_quota < session_size)
			throw Insufficient_ram_quota();


		/***********************
		 ** Find ROM in cache **
		 ***********************/

		Session_space::Id  const id { pid.value };

		Session_label const label = label_from_args(args.string());
		Path          const path(label.last_element().string());

		/* lookup the ROM in the cache */
		Cached_rom *rom = nullptr;
		cache.for_each<Cached_rom&>([&] (Cached_rom &other) {
			if (!rom && other.path == path)
				rom = &other;
		});

		if (rom) {
			/* Create new RPC object */
			Session_component *session = new (heap)
				Session_component(*rom, sessions, id);
			if (session_diag_from_args(args.string()).enabled)
				log("deliver ROM \"", label, "\"");
			env.parent().deliver_session_cap(pid, env.ep().manage(*session));

			return;
		}

		/* find matching transfer */
		bool pending = false;
		transfers.for_each<Transfer&>([&] (Transfer &other) {
			pending |= other.matches(path); });

		if (pending) /* wait until transfer completes */
			return;

		/* initiate new transfer or throw Service_denied */
		try_transfer(path, [&] () {
			Transfer *transfer = new (heap)
				Transfer(env, fs, transfers, path, pid);
			process_transfer(*transfer);
		});
	}

	/**
	 * Destroy a closed session component
	 */
	void handle_session_close(Parent::Server::Id pid) override
	{
		Session_space::Id id { pid.value };
		sessions.apply<Session_component&>(
			id, [&] (Session_component &session)
		{
			env.ep().dissolve(session);
			destroy(heap, &session);
			env.parent().session_response(pid, Parent::SESSION_CLOSED);
		});
	}

	/**
	 * Handle pending READ packets
	 */
	void handle_packets()
	{
		Tx_source &source = *fs.tx();

		while (source.ack_avail()) {
			File_system::Packet_descriptor pkt = source.get_acked_packet();
			if (pkt.operation() != File_system::Packet_descriptor::READ) continue;

			bool stray_pkt = true;

			/* find the appropriate session */
			transfers.apply<Transfer&>(
				Transfer_space::Id{pkt.handle().value}, [&] (Transfer &transfer)
			{
				try {
					try_transfer(transfer.final_path, [&] () {
						transfer.process_packet(pkt);
						process_transfer(transfer);
						stray_pkt = false;
					});
				} catch (Service_denied) {
					env.parent().session_response(
						transfer.request_id, Parent::SERVICE_DENIED);
					destroy(heap, &transfer);
				}
			});

			if (stray_pkt)
				source.release_packet(pkt);
		}
	}

	Main(Genode::Env &env) : env(env)
	{
		fs.sigh_ack_avail(packet_handler);

		/* process any requests that have already queued */
		session_requests.schedule();
	}
};


void Component::construct(Genode::Env &env)
{
	static Cached_fs_rom::Main inst(env);
	env.parent().announce("ROM");
}
