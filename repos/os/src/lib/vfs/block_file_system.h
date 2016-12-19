/*
 * \brief  Block device file system
 * \author Josef Soentgen
 * \author Norman Feske
 * \author Emery Hemingway
 * \date   2013-12-20
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_
#define _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_

#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <vfs/single_file_system.h>

namespace Vfs { class Block_file_system; }


class Vfs::Block_file_system : public Single_file_system
{
	private:

		typedef Genode::String<64> Label;

		Genode::Allocator_avl       _tx_block_alloc;
		Block::Connection           _block;
		Genode::size_t              _block_size;
		Block::sector_t             _block_count;
		Block::Session::Operations  _block_ops;
		Block::Session::Tx::Source &_tx_source;
		bool                        _readable;
		bool                        _writeable;

		Genode::Signal_handler<Block_file_system> _ack_handler;

		struct Handle final : Vfs_handle, Genode::List<Handle>::Element
		{
			Block::Packet_descriptor::Opcode op =
				Block::Packet_descriptor::Opcode::END;

			Block::sector_t block_number = 0;
			Genode::size_t  block_count  = 0;
			Genode::off_t   op_offset    = 0;

			bool bounce_write = false;

			using Vfs_handle::Vfs_handle;
		};

		Genode::List<Handle> _handles;

		void _process_ack(Block::Packet_descriptor p)
		{
			using namespace Block;
			typedef Block::Packet_descriptor::Opcode Opcode;

			if (p.operation() == Opcode::END) {
				Genode::warning("invalid Block packet received client side");
				_tx_source.release_packet(p);
				return;
			}

			_handles.for_each([&] (Handle &h) {

				/* does the handle op fit inside the packet op? */
				int const block_offset = p.block_number()-h.block_number;
				if ((h.op == p.operation()) && (block_offset >= 0) &&
					(h.block_count >= (p.block_count()-block_offset)))
				{
					Genode::off_t const op_offset =
						block_offset*_block_size + h.op_offset;

					Callback::Status const s = p.succeeded() ?
						Callback::COMPLETE : Callback::ERR_IO;
					switch (p.operation()) {

					/* callback op lengths are rounded up, deal with it */
					case Opcode::READ:
						if (h.bounce_write) {
							/* write to the packet and resubmit */
							h.write_callback(_tx_source.packet_content(p)+op_offset,
							                 h.block_count*_block_size, s);
							_tx_source.submit_packet(Packet_descriptor(
								p, p.operation(), p.block_number(), p.block_count()));
							h.bounce_write = false;
						} else {
							/* read, release, and complete */
							h.read_callback(_tx_source.packet_content(p)+op_offset,
							                h.block_count*_block_size, s);
							_tx_source.release_packet(p);
							p = Packet_descriptor();
							h.op = Opcode::END;
							h.read_status(s);
						}
						break;

					case Opcode::WRITE:
						/* release and complete */
						_tx_source.release_packet(p);
						p = Packet_descriptor();
						h.op = Opcode::END;
						h.write_status(s);
						break;

					case Opcode::END: break;

					}
				}

			});

			if (p.operation() != Opcode::END) {
				Genode::warning("VFS: received block packet without handle");
				_tx_source.release_packet(p);
			}
		}

		void _process_acks()
		{
			while(_tx_source.ack_avail())
				_process_ack(_tx_source.get_acked_packet());
		}

	public:

		Block_file_system(Genode::Env &env,
		                  Genode::Allocator &alloc,
		                  Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_BLOCK_DEVICE, name(),
			                  config, OPEN_MODE_RDWR),
			_tx_block_alloc(&alloc),
			_block(env, &_tx_block_alloc,
			      config.attribute_value("buffer_size", Genode::size_t(Block::DEFAULT_TX_BUF_SIZE)),
			      config.attribute_value("label", Label()).string()),
			_tx_source(*_block.tx()),
			_readable(false),
			_writeable(false),
			_ack_handler(env.ep(), *this, &Block_file_system::_process_acks)
		{
			_block.info(&_block_count, &_block_size, &_block_ops);

			_readable  = _block_ops.supported(Block::Packet_descriptor::READ);
			_writeable = _block_ops.supported(Block::Packet_descriptor::WRITE);

			_block.tx_channel()->sigh_ack_avail(_ack_handler);
		}

		static char const *name() { return "block"; }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result const result = Single_file_system::stat(path, out);
			out.size = _block_count * _block_size;
			return result;
		}

		Open_result open(char const  *path, unsigned mode,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			if (mode & Vfs::Directory_service::OPEN_MODE_CREATE)
				return OPEN_ERR_EXISTS;

			Handle *handle = new (alloc) Handle(*this, *this, alloc, 0);
			_handles.insert(handle);
			*out_handle = handle;
			return OPEN_OK;
		}

		void close(Vfs_handle *vfs_handle) override
		{
			Handle *handle = static_cast<Handle *>(vfs_handle);
			if (handle && (&handle->ds() == this)) {
				_handles.remove(handle);
				destroy(handle->alloc(), handle);
			}
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		void write(Vfs_handle *vfs_handle, file_size count) override
		{
			using namespace Block;

			if (!_writeable) {
				Genode::warning("block device is not writeable");
				return vfs_handle->write_status(Callback::ERR_INVALID);
			}

			Handle *handle = static_cast<Handle *>(vfs_handle);
			if (handle->op != Packet_descriptor::END) {
				Genode::warning("VFS: refusing concurrent write on block handle");
				return handle->write_status(Callback::ERR_INVALID);
			}

			if (!_tx_source.ready_to_submit()) {
				Genode::warning("VFS: block packet queue congested");
				return handle->write_status(Callback::ERR_IO);
			}

			Genode::size_t const count_offset = count % _block_size;
			if (count_offset)
				count += _block_size - count_offset;

			Packet_descriptor raw_packet;
			for (;;) {
				try {
					raw_packet = _tx_source.alloc_packet(count);
					break;
				} catch (...) {
					count /= 2;
					if (count < _block_size)
						return handle->write_status(Callback::ERR_IO);
				}
			}

			Genode::size_t  const block_count = count / _block_size;
			Block::sector_t const block_number = vfs_handle->seek() / _block_size;
			Block::sector_t const op_offset = vfs_handle->seek() % _block_size;

			if (op_offset || count_offset) {

				/* not aligned, read first */
				_tx_source.submit_packet(Packet_descriptor(
					raw_packet, Packet_descriptor::READ, block_number, block_count));
				handle->bounce_write = true;

			} else {

				handle->write_callback(
					_tx_source.packet_content(raw_packet), count, Callback::PARTIAL);
				_tx_source.submit_packet(Packet_descriptor(
					raw_packet, Packet_descriptor::WRITE, block_number, block_count));
				handle->bounce_write = false;

			}

			handle->block_number = block_number;
			handle->block_count  = block_count;
			handle->op_offset    = op_offset;
		}

		void read(Vfs_handle *vfs_handle, file_size count) override
		{
			using namespace Block;

			if (!_readable) {
				Genode::warning("block device is not readeable");
				return vfs_handle->read_status(Callback::ERR_INVALID);
			}

			Handle *handle = static_cast<Handle *>(vfs_handle);
			if (handle->op != Packet_descriptor::END) {
				Genode::warning("VFS: refusing concurrent read on block handle");
				return handle->read_status(Callback::ERR_INVALID);
			}

			if (!_tx_source.ready_to_submit()) {
				Genode::warning("VFS: block packet queue congested");
				return handle->read_status(Callback::ERR_IO);
			}

			Genode::size_t const count_offset = count % _block_size;
			if (count_offset)
				count += _block_size - count_offset;

			Packet_descriptor raw_packet;
			for (;;) {
				try {
					raw_packet = _tx_source.alloc_packet(count);
					break;
				} catch (...) {
					count /= 2;
					if (count < _block_size)
						return handle->write_status(Callback::ERR_IO);
				}
			}

			Genode::size_t  const block_count = count / _block_size;
			Block::sector_t const block_number = vfs_handle->seek() / _block_size;
			Block::sector_t const op_offset = vfs_handle->seek() % _block_size;

			_tx_source.submit_packet(Packet_descriptor(
				raw_packet, Packet_descriptor::READ, block_number, block_count));

			handle->block_number = block_number;
			handle->block_count  = block_count;
			handle->op_offset    = op_offset;
			handle->bounce_write = false;
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			return FTRUNCATE_OK;
		}

		Ioctl_result ioctl(Vfs_handle *vfs_handle, Ioctl_opcode opcode, Ioctl_arg,
		                   Ioctl_out &out) override
		{
			switch (opcode) {
			case IOCTL_OP_DIOCGMEDIASIZE:

				out.diocgmediasize.size = _block_count * _block_size;
				return IOCTL_OK;

			default:

				Genode::warning("invalid ioctl request ", (int)opcode);
				break;
			}

			/* never reached */
			return IOCTL_ERR_INVALID;
		}
};

#endif /* _INCLUDE__VFS__BLOCK_FILE_SYSTEM_H_ */
