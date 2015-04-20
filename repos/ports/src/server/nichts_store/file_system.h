/*
 * \brief  File system proxy
 * \author Emery Hemingway
 * \date   2015-04-20
 *
 * This File_system::Session_component allows clients
 * to naively import file trees and access filtered views
 */

#ifndef _NICHTS_STORE__FILE_SYSTEM_H_
#define _NICHTS_STORE__FILE_SYSTEM_H_

#include <file_system_session/file_system_session.h>

namespace File_system { class Session_component; }

class File_system::Session_component : public Genode::Rpc_object<Session>
{
	private:

		Node_handle_registry  _handle_registry;
		Allocator            &_md_alloc;

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
				res_length = node.write((char *)content, length, offset);
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
				Node *node = _handle_registry.lookup(packet.handle());
				Node_lock_guard guard(*node);

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


		
}

#endif