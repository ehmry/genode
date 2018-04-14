/*
 * \brief  Serve blocks from a file system session
 * \author Emery Hemingway
 * \date   2015-09-25
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <block_session/rpc_object.h>
#include <block/driver.h>
#include <os/path.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/allocator_avl.h>
#include <base/heap.h>
#include <base/component.h>


namespace Fs_blk {

	using namespace Genode;
	using namespace Block;

	struct Block_buffer;
	class  Session_component;
	class  Root_component;

	typedef Genode::Path<File_system::MAX_PATH_LEN> Path;
}


struct Fs_blk::Block_buffer
{
	Attached_ram_dataspace buffer_ds;

	Block_buffer(Genode::Env &env, size_t buffer_size)
	: buffer_ds(env.pd(), env.rm(), buffer_size) { }
};


class Fs_blk::Session_component final : private Block_buffer,
                                        public Block::Session_rpc_object
{
	private:

		Block::Packet_descriptor _pending[File_system::Session::TX_QUEUE_SIZE];

		Genode::Entrypoint        &_ep;
		Genode::Heap              &_heap;
		Genode::Allocator_avl      _fs_tx_alloc { &_heap };
		File_system::Connection    _fs;
		Io_signal_handler<Session_component> _fs_handler;
		Io_signal_handler<Session_component> _blk_handler;
		File_system::Node_handle   _handle { };
		Genode::size_t       const _blk_size;
		Block::sector_t            _blk_count = 0;
		Block::Session::Operations _ops { };
		Genode::size_t             _pending_count = 0; /* pending index */
		bool                       _pending_sync = false;

		void _process_client()
		{
			Block::Session::Tx::Sink         &sink   = *tx_sink();
			File_system::Session::Tx::Source &source = *_fs.tx();

			_pending[_pending_count] = sink.get_packet();
			Block::Packet_descriptor &blk_packet = _pending[_pending_count++];

			File_system::seek_off_t const start = blk_packet.block_number() * _blk_size;
			size_t const len = blk_packet.block_count() * _blk_size;

			blk_packet.succeeded(false);

			File_system::Packet_descriptor::Opcode op;
			switch (blk_packet.operation()) {
			case Block::Packet_descriptor::Opcode::READ:
				op = File_system::Packet_descriptor::Opcode::READ; break;
			case Block::Packet_descriptor::Opcode::WRITE:
				op = File_system::Packet_descriptor::Opcode::WRITE; break;
			default:
				sink.acknowledge_packet(_pending[--_pending_count]);
				return;
			}

			File_system::Packet_descriptor fs_packet(
				source.alloc_packet(len), _handle, op,
				len, File_system::seek_off_t(start));

			if (op == File_system::Packet_descriptor::WRITE)
				memcpy(source.packet_content(fs_packet),
				       sink.packet_content(blk_packet),
				       fs_packet.length());

			source.submit_packet(fs_packet);
		}

		void _process_server()
		{
			File_system::Session::Tx::Source &source = *_fs.tx();
			Block::Session::Tx::Sink         &sink   = *tx_sink();

			File_system::Packet_descriptor fs_packet = source.get_acked_packet();

			Block::Packet_descriptor::Opcode op;
			switch (fs_packet.operation()) {
			case File_system::Packet_descriptor::Opcode::READ:
				op = Block::Packet_descriptor::Opcode::READ; break;
			case File_system::Packet_descriptor::Opcode::WRITE:
				op = Block::Packet_descriptor::Opcode::WRITE; break;
			case File_system::Packet_descriptor::Opcode::SYNC:
				_pending_sync = false;
				return;
			default:
				source.release_packet(fs_packet);
				return;
			}

			Block::sector_t const blk_num = fs_packet.position() / _blk_size;
			Genode::size_t  const blk_cnt = fs_packet.length() / _blk_size;

			if (_pending_count < 1)
				error("got a packet but nothing pending");

			for (size_t i = 0; i < _pending_count; ++i) {
				Block::Packet_descriptor &blk_packet = _pending[i];
				if (blk_packet.operation() == op &&
				    blk_packet.block_number() == blk_num &&
				    blk_packet.block_count()  >= blk_cnt)
				{
					/* create a new packet with the length from the FS */
					Block::Packet_descriptor ack_pkt(blk_packet, op, blk_num, blk_cnt);
					ack_pkt.succeeded(fs_packet.succeeded());

					if (op == Block::Packet_descriptor::Opcode::READ && fs_packet.succeeded()) {
						memcpy(sink.packet_content(ack_pkt),
						       source.packet_content(fs_packet),
						       blk_cnt * _blk_size);
					}

					sink.acknowledge_packet(ack_pkt);

					/* shift the pending queue down if a gap forms */
					if (i != _pending_count)
						memcpy(&_pending[i],
						       &_pending[i+1],
						       (_pending_count-i)*sizeof(Block::Packet_descriptor));

					--_pending_count;
					source.release_packet(fs_packet);
					return;
				}
			}
		}

		void _process_blk()
		{
			File_system::Session::Tx::Source &source = *_fs.tx();
			Block::Session::Tx::Sink         &sink = *tx_sink();
			while (sink.packet_avail() && source.ready_to_submit())
				_process_client();
		}

		void _process_fs()
		{
			Block::Session::Tx::Sink         &sink = *tx_sink();
			File_system::Session::Tx::Source &source = *_fs.tx();
			while (source.ack_avail() && sink.ready_to_ack())
				_process_server();
		}

		/**
		 * Block until all packets process and file is synced.
		 */
		void _fs_sync()
		{
			while (_pending_count > 0)
				_ep.wait_and_dispatch_one_io_signal();

			File_system::Packet_descriptor pkt(
				File_system::Packet_descriptor(), _handle,
				File_system::Packet_descriptor::SYNC, 0, 0);

			_pending_sync = true;
			_fs.tx()->submit_packet(pkt);
			while (_pending_sync)
				_ep.wait_and_dispatch_one_io_signal();
		}

		Path _file_root(char const *file_path)
		{
			Path r(file_path);
			r.strip_last_element();
			return r;
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::Env         &env,
		                  Genode::Heap        &heap,
		                  size_t               tx_buf_size,
		                  size_t               block_size,
		                  char const          *file_path,
		                  bool                 writeable)
		:
			Block_buffer(env, tx_buf_size),
			Session_rpc_object(env.rm(), Block_buffer::buffer_ds.cap(),
			                   env.ep().rpc_ep()),
			_ep(env.ep()), _heap(heap),
			_fs(env, _fs_tx_alloc, "", _file_root(file_path).string(),
			    writeable, tx_buf_size),
			_fs_handler( env.ep(), *this, &Session_component::_process_fs),
			_blk_handler(env.ep(), *this, &Session_component::_process_blk),
			_blk_size(block_size)
		{
			using namespace File_system;

			/* the File_system session is rooted at the parent directory of the file */
			Path file_name(file_path);
			file_name.keep_only_last_element();
			char const *name = file_name.string()+1;

			try {
				Dir_handle dir = _fs.dir("/", false);
				Handle_guard guard(_fs, dir);

				if (writeable) {
					try {
						_handle = _fs.file(dir, name, READ_WRITE, false);
						_ops.set_operation(Block::Packet_descriptor::READ);
						_ops.set_operation(Block::Packet_descriptor::WRITE);
					} catch (Permission_denied) {
						try {
							_handle = _fs.file(dir, name, READ_ONLY, false);
							_ops.set_operation(Block::Packet_descriptor::READ);
						} catch (Permission_denied) {
							/* not likely, but still supported */
							_handle = _fs.file(dir, name, WRITE_ONLY, false);
							_ops.set_operation(Block::Packet_descriptor::WRITE);
						}
					}
				} else {
					_handle = _fs.file(dir, name, READ_ONLY, false);
					_ops.set_operation(Block::Packet_descriptor::READ);
				}
			} catch (...) {
				error("failed to open ", file_path);
				throw Service_denied();
			}

			File_system::Status st = _fs.status(_handle);
			_blk_count = st.size / block_size;
			if (st.size % block_size) /* round up */
				++_blk_count;

			/* register signal handlers */
			_fs.sigh_ack_avail(_fs_handler);
			_tx.sigh_packet_avail(_blk_handler);
		}

		~Session_component() { _fs_sync(); }

		/*****************************
		 ** Block session interface **
		 *****************************/

		void info(sector_t       *blk_count,
		          Genode::size_t *blk_size,
		          Operations     *ops)
		{
			*blk_count = _blk_count;
			*blk_size  = _blk_size;
			*ops       = _ops;
		}

		void sync() override { _fs_sync(); }
};


class Fs_blk::Root_component final :
	public Genode::Root_component<Fs_blk::Session_component, Genode::Single_client>
{
	private:

		Genode::Env  &_env;
		Genode::Heap &_heap;

		Attached_rom_dataspace _config_rom { _env, "config" };

	protected:

		Session_component *_create_session(char const *args) override
		{
			Session_label const label = label_from_args(args);

			long block_size = 512;
			bool writeable = false;

			_config_rom.update();
			Xml_node config_node = _config_rom.xml();

			String<File_system::MAX_PATH_LEN> path;
			try {
				Session_policy const policy(label, config_node);
				policy.attribute("file").value(&path);
			} catch (...) {
				error("no valid policy for '", label, "'");
				throw Service_denied();
			}
			block_size = config_node.attribute_value("block_size", block_size);
			writeable  = config_node.attribute_value("writeable", false);

			/*
			 * Check that there is sufficient quota, but the client is not
			 * required to pay for the entire cost of the session for two
			 * reasons.
			 *
			 * First, we only serve a single client at a time so a denial of
			 * service is not issue. If the quota is deficient the session
			 * is denied and the parent is not consulted.
			 *
			 * Second, we maintain a packet buffer of equal size with the
			 * backend that we do not expect the client to account for.
			 * This simplifies the implementation because any packet
			 * allocation at the client can be matched at the backend.
			 */

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).aligned_size();
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").aligned_size();

			if (!tx_buf_size) {
				error("invalid buffer size");
				throw Service_denied();
			}

			size_t avail = ram_quota + _env.pd().avail_ram().value;

			size_t session_size = max((size_t)4096, sizeof(Session_component));

			if ((avail < session_size) ||
			    (tx_buf_size*2 > avail - session_size))
			{
				error("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, (tx_buf_size*2 + session_size) - avail);
				throw Service_denied();
			}

			return new (_heap)
				Session_component(_env, _heap, tx_buf_size, block_size,
				                  path.string(), writeable);
		}

	public:

		/**
		 * Constructor
		 */
		Root_component(Genode::Env &env, Genode::Heap &heap)
		:
			Genode::Root_component<Fs_blk::Session_component, Genode::Single_client>(
				env.ep(), heap),
			_env(env), _heap(heap)
		{ }
};


void Component::construct(Genode::Env &env)
{
	static Genode::Heap           heap(env.pd(), env.rm());
	static Fs_blk::Root_component root(env, heap);

	env.parent().announce(env.ep().manage(root));
}
