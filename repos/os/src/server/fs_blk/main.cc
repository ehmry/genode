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

		enum { PENDING_QUEUE_COUNT = File_system::Session::TX_QUEUE_SIZE };
		Block::Packet_descriptor _pending[PENDING_QUEUE_COUNT];

		Genode::Heap              &_heap;
		Genode::Allocator_avl      _fs_tx_alloc { &_heap };
		File_system::Connection    _fs;
		Signal_handler<Session_component> _fs_handler;
		Signal_handler<Session_component> _blk_handler;
		File_system::File_handle   _handle { ~0U };
		Genode::size_t       const _blk_size;
		Block::sector_t            _blk_count = 0;
		Block::Session::Operations _ops { };
		Genode::size_t             _pending_count = 0; /* pending index */
		bool                       _pending_sync = false;

		void _process_blk_pkt(Block::Packet_descriptor const blk_packet)
		{
			File_system::Session::Tx::Source &source = *_fs.tx();
			Block::Session::Tx::Sink         &sink = *tx_sink();

			File_system::seek_off_t start = blk_packet.block_number() * _blk_size;
			size_t const len = blk_packet.block_count() * _blk_size;

			File_system::Packet_descriptor::Opcode op;
			switch (blk_packet.operation()) {
			case Block::Packet_descriptor::Opcode::READ:
				op = File_system::Packet_descriptor::Opcode::READ; break;
			case Block::Packet_descriptor::Opcode::WRITE:
				op = File_system::Packet_descriptor::Opcode::WRITE; break;
			default:
				throw ~0;
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

		void _process_blk()
		{
			File_system::Session::Tx::Source &source = *_fs.tx();
			Block::Session::Tx::Sink         &sink = *tx_sink();
			while (sink.packet_avail()
			    && source.ready_to_submit()
			    && _pending_count < PENDING_QUEUE_COUNT)
			{
				Block::Packet_descriptor pkt = sink.get_packet();
				pkt.succeeded(false);

				if (pkt.operation() > Block::Packet_descriptor::Opcode::WRITE
				 || pkt.block_number() + pkt.block_count() > _blk_count
				 || pkt.block_count() * _blk_size > pkt.size())
				{
					sink.acknowledge_packet(pkt);
				} else {
					for (int i = 0; i < PENDING_QUEUE_COUNT; ++i) {
						if (_pending[i].size() == 0) {
							_pending[i] = pkt;
							break;
						}
					}
					++_pending_count;
					_process_blk_pkt(pkt);
				}
			}
		}

		void _process_fs_packet()
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
			default:
				source.release_packet(fs_packet);
				return;
			}

			Block::sector_t const blk_num = fs_packet.position() / _blk_size;

			if (_pending_count < 1)
				error("got a packet but nothing pending");

			for (int i = 0; i < PENDING_QUEUE_COUNT; ++i) {
				/* process the first queue item with the same offset */
				if (_pending[i].operation() == op &&
				    _pending[i].block_number() == blk_num)
				{
					size_t byte_count = min(
						_pending[i].size(), fs_packet.length());
					size_t blk_count = min(
						_pending[i].block_count(), byte_count / _blk_size);

					/* create a new packet with the length from the FS */
					Block::Packet_descriptor ack_pkt(
						_pending[i], op, blk_num, blk_count);
					ack_pkt.succeeded(fs_packet.succeeded());

					/* wipe the queue entry */
					_pending[i] = Block::Packet_descriptor();
					--_pending_count;

					if (op == Block::Packet_descriptor::Opcode::READ) {
						/* zero any trailing content (last block of file) */
						if (byte_count < ack_pkt.size()) {
							memset(sink.packet_content(ack_pkt),
							       0x00, ack_pkt.size());
						}

						memcpy(sink.packet_content(ack_pkt),
						       source.packet_content(fs_packet),
						       byte_count);
					}

					/* free packets */
					sink.acknowledge_packet(ack_pkt);
					source.release_packet(fs_packet);
					break;
				}
			}
		}

		void _process_fs()
		{
			Block::Session::Tx::Sink         &sink = *tx_sink();
			File_system::Session::Tx::Source &source = *_fs.tx();
			while (source.ack_avail() && sink.ready_to_ack())
				_process_fs_packet();
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
		Session_component(Genode::Env  &env,
		                  Genode::Heap &heap,
		                  size_t        tx_buf_size,
		                  size_t        block_size,
		                  size_t        block_count,
		                  char const   *file_path,
		                  bool          writeable)
		:
			Block_buffer(env, tx_buf_size),
			Session_rpc_object(env.rm(), Block_buffer::buffer_ds.cap(),
			                   env.ep().rpc_ep()),
			_heap(heap),
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
					}

					catch (Lookup_failed) {
						_handle = _fs.file(dir, name, READ_WRITE, true);
						_ops.set_operation(Block::Packet_descriptor::READ);
						_ops.set_operation(Block::Packet_descriptor::WRITE);
					}

					catch (Permission_denied) {
						try {
							_handle = _fs.file(dir, name, READ_ONLY, false);
							_ops.set_operation(Block::Packet_descriptor::READ);
						}

						catch (Permission_denied) {
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
				error("failed to open ", file_path,
				      writeable ? " writeable " : " read-only");
				throw Service_denied();
			}

			File_system::Status st = _fs.status(_handle);
			_blk_count = st.size / _blk_size;
			if (st.size % block_size) /* round up */
				++_blk_count;

			if (writeable && block_count > 0 && block_count != _blk_count) {
				file_size_t file_size = _blk_size*block_count;
				try { _fs.truncate(_handle, file_size); }
				catch (...) {
					error("failed to resize ", file_path, " to ", file_size, " bytes");
					throw Service_denied();
				}
				_blk_count = file_size / _blk_size;
			}

			/* register signal handlers */
			_fs.sigh_ack_avail(_fs_handler);
			_tx.sigh_packet_avail(_blk_handler);

			for (int i = 0; i < PENDING_QUEUE_COUNT; ++i)
				_pending[i] = Block::Packet_descriptor();
		}

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

		void sync() override
		{
			if (_fs.tx()->ready_to_submit()) {
				_fs.tx()->submit_packet(File_system::Packet_descriptor(
					File_system::Packet_descriptor(), _handle,
					File_system::Packet_descriptor::SYNC, 0, 0));
				_pending_sync = true;
			} else {
				error("cannot sync, File_system submit queue is full");
			}
		}
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

			long block_size  = 512;
			long block_count = 0;
			bool writeable   = false;

			_config_rom.update();

			String<File_system::MAX_PATH_LEN> path;
			try {
				Session_policy const policy(label, _config_rom.xml());
				policy.attribute("file").value(&path);
				block_size  = policy.attribute_value("block_size",  block_size);
				block_count = policy.attribute_value("block_count", block_count);
				writeable   = policy.attribute_value("writeable", false);
			} catch (...) {
				error("no valid policy for '", label, "'");
				throw Service_denied();
			}

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
				Session_component(_env, _heap, tx_buf_size,
				                  block_size, block_count,
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
