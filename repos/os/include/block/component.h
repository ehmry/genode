/*
 * \brief  Block-session component
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2011-05-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BLOCK__COMPONENT_H_
#define _INCLUDE__BLOCK__COMPONENT_H_

#include <os/session_policy.h>
#include <root/component.h>
#include <os/signal_rpc_dispatcher.h>
#include <block/driver.h>
#include <base/log.h>

namespace Block {

	using namespace Genode;

	struct Policy;
	class Session_component_base;
	class Session_component;
	class Root;

	typedef List<Session_component> Session_list;

};


/**
 * Convenience struct for session policy
 */
struct Block::Policy
{
	sector_t offset, span;
	bool readable, writeable;

	Policy() : offset(0), span(0), readable(true), writeable(true) { }

	/**
	 * Parse session constraints from a config policy
	 * and client arguments.
	 */
	Policy(Genode::Session_policy const &policy, char const *args)
	:
		offset(Arg_string::find_arg(args, "offset").ulong_value(0)),
		span  (Arg_string::find_arg(args, "span"  ).ulong_value(0)),

		/* permissive for now */
		readable(policy.attribute_value("readable",   true) ?
			Arg_string::find_arg(args, "readable").bool_value(true)  : false),
		writeable(policy.attribute_value("writeable", true) ?
			Arg_string::find_arg(args, "writeable").bool_value(true) : false)
	{ }

	/**
	 * Parse session constraints from client arguments.
	 */
	Policy(char const *args)
	:
		offset(Arg_string::find_arg(args, "offset").ulong_value(0)),
		span  (Arg_string::find_arg(args, "span"  ).ulong_value(0)),

		/* permissive for now */
		readable(Arg_string::find_arg(args, "readable").bool_value(true)),
		writeable(Arg_string::find_arg(args, "writeable").bool_value(true))
	{ }
};


/**
 * We have a hen and egg situation that makes this base class necessary.
 * The Block::Session_rpc_object construction depends on a dataspace for
 * the packet stream. The dataspace on the other hand is constructed by
 * the driver, which is created on demand when creating a session.
 * When creating the driver, and dataspace outside the Session_component
 * constructor within _create_session of the root component, we would have
 * to destroy the driver and dataspace within the destructor body of
 * Session_component, which will lead to problems, because the packet stream
 * destructors will be called after the shared memory already vanished.
 */
class Block::Session_component_base
{
	protected:

		Driver_factory          &_driver_factory;
		Driver                  &_driver;
		Ram_dataspace_capability _rq_ds;

		Session_component_base(Driver_factory &factory, size_t tx_buf_size)
		: _driver_factory(factory),
		  _driver(*factory.create()),
		  _rq_ds(_driver.alloc_dma_buffer(tx_buf_size)) {}

		~Session_component_base()
		{
			_driver.free_dma_buffer(_rq_ds);
			_driver_factory.destroy(&_driver);
		}
};


class Block::Session_component : public Block::Session_component_base,
                                 public Block::Driver_session,
                                 public Block::Session_list::Element
{
	private:

		Genode::Session_label          const _label;
		addr_t                               _rq_phys;
		Signal_rpc_member<Session_component> _sink_ack;
		Signal_rpc_member<Session_component> _sink_submit;
		bool                                 _req_queue_full;
		bool                                 _ack_queue_full;
		Packet_descriptor                    _p_to_handle;
		unsigned                             _p_in_fly;

		sector_t const _offset;
		sector_t const _span;
		bool     const _readable;
		bool     const _writeable;

		/**
		 * Acknowledge a packet already handled
		 */
		inline void _ack_packet(Packet_descriptor &packet)
		{
			if (!tx_sink()->ready_to_ack())
				PERR("Not ready to ack!");

			tx_sink()->acknowledge_packet(packet);
			_p_in_fly--;
		}

		/**
		 * Range check packet request
		 */
		inline bool _range_check(Packet_descriptor &p) {
			return p.block_number() + p.block_count() - 1
			       < _driver.block_count(); }

		/**
		 * Handle a single request
		 */
		void _handle_packet(Packet_descriptor packet)
		{
			_p_to_handle = packet;
			_p_to_handle.succeeded(false);

			/* ignore invalid packets */
			if (!packet.size() || !_range_check(_p_to_handle)) {
				_ack_packet(_p_to_handle);
				return;
			}

			/* enforce session constraints */
			sector_t       const block_number =
				min(packet.block_number() + _offset, _span);
			Genode::size_t const block_count =
				min(packet.block_count(), _span - block_number);

			try {
				switch (_p_to_handle.operation()) {

				case Block::Packet_descriptor::READ:
					if (!_readable) break;

					if (_driver.dma_enabled())
						_driver.read_dma(block_number, block_count,
						                 _rq_phys + packet.offset(),
						                 _p_to_handle);
					else
						_driver.read(block_number, block_count,
						             tx_sink()->packet_content(packet),
						             _p_to_handle);
					break;

				case Block::Packet_descriptor::WRITE:
					if (!_writeable) break;

					if (_driver.dma_enabled())
						_driver.write_dma(block_number, block_count,
						                  _rq_phys + packet.offset(),
						                  _p_to_handle);
					else
						_driver.write(block_number, block_count,
						              tx_sink()->packet_content(packet),
						              _p_to_handle);
					break;

				default:
					throw Driver::Io_error();
				}
			} catch (Driver::Request_congestion) {
				_req_queue_full = true;
			} catch (Driver::Io_error) {
				_ack_packet(_p_to_handle);
			}
		}

		/**
		 * Triggered when a packet was placed into the empty submit queue
		 */
		void _packet_avail(unsigned)
		{
			/*
			 * as long as more packets are available, and we're able to ack
			 * them, and the driver's request queue isn't full,
			 * direct the packet request to the driver backend
			 */
			for (_ack_queue_full = (_p_in_fly >= tx_sink()->ack_slots_free());
			     !_req_queue_full && !_ack_queue_full
			     && tx_sink()->packet_avail();
				 _ack_queue_full = (++_p_in_fly >= tx_sink()->ack_slots_free()))
				_handle_packet(tx_sink()->get_packet());
		}

		/**
		 * Triggered when an ack got removed from the full ack queue
		 */
		void _ready_to_ack(unsigned) { _packet_avail(0); }

	public:

		/**
		 * Constructor
		 *
		 * \param driver_factory  factory to create and destroy driver objects
		 * \param ep              entrypoint handling this session component
		 * \param buf_size        size of buffer shared with client
		 * \param policy          session region and read-write policy
		 * \param label           session label for diagnostics
		 */
		Session_component(Driver_factory              &driver_factory,
		                  Genode::Entrypoint          &ep,
		                  size_t                       buf_size,
		                  Block::Policy         const &policy,
		                  Genode::Session_label const &label)
		: Session_component_base(driver_factory, buf_size),
		  Driver_session(_rq_ds, ep.rpc_ep()),
		  _label(label),
		  _rq_phys(Dataspace_client(_rq_ds).phys_addr()),
		  _sink_ack(ep, *this, &Session_component::_ready_to_ack),
		  _sink_submit(ep, *this, &Session_component::_packet_avail),
		  _req_queue_full(false),
		  _p_in_fly(0),
		  /* check these in a bit */
		  _offset(policy.offset),
		  _span(policy.span ? policy.span : _driver.block_count()),
		  _readable(policy.readable),
		  _writeable(policy.writeable)
		{
			if (_offset >= _driver.block_count()) {
				error("session block offset (", _offset, ") "
				      "exceeds block count (", _driver.block_count(), "), "
				      "denying '", label, "'");
				throw Genode::Root::Unavailable();
			}
			if (_span > (_driver.block_count() - _offset)) {
				error("session block span (",  _span, ") "
				      "exceeds size of device (", _driver.block_count(), "), "
				      "denying '", label, "'");
				throw Genode::Root::Unavailable();
			}

			_tx.sigh_ready_to_ack(_sink_ack);
			_tx.sigh_packet_avail(_sink_submit);

			_driver.session(this);
		}

		~Session_component() { _driver.session(nullptr); }

		Genode::Session_label const &label() const { return _label; }

		bool writeable() const { return _writeable; }

		sector_t offset() const { return _offset; }
		sector_t   span() const { return   _span; }

		/**
		 * Acknowledges a packet processed by the driver to the client
		 *
		 * \param packet   the packet to acknowledge
		 * \param success  indicated whether the processing was successful
		 *
		 * \throw Ack_congestion
		 */
		void ack_packet(Packet_descriptor &packet, bool success)
		{
			packet.succeeded(success);
			_ack_packet(packet);

			if (!_req_queue_full && !_ack_queue_full)
				return;

			/*
			 * when the driver's request queue was full,
			 * handle last unprocessed packet taken out of submit queue
			 */
			if (_req_queue_full) {
				_req_queue_full = false;
				_handle_packet(_p_to_handle);
			}

			/* resume packet processing */
			_packet_avail(0);
		}


		/*******************************
		 **  Block session interface  **
		 *******************************/

		void info(sector_t *blk_count, size_t *blk_size,
		          Operations *ops)
		{
			*blk_count = _driver.block_count();
			*blk_size  = _driver.block_size();
			*ops       = _driver.ops();
		}

		void sync() { _driver.sync(); }
};


/**
 * Root component, handling new session requests
 */
class Block::Root : public Genode::Root_component<Block::Session_component>
{
	private:

		Driver_factory     &_driver_factory;
		Genode::Entrypoint &_ep;
		Session_list        _sessions;

	protected:

		Session_component *_create_session(const char *args) override
		{
			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			/* delete ram quota by the memory needed for the session */
			size_t session_size = max((size_t)4096,
			                          sizeof(Session_component)
			                          + sizeof(Allocator_avl));
			if (ram_quota < session_size)
				throw Root::Quota_exceeded();

			/*
			 * Check if donated ram quota suffices for both
			 * communication buffers. Also check both sizes separately
			 * to handle a possible overflow of the sum of both sizes.
			 */
			if (tx_buf_size > ram_quota - session_size) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, tx_buf_size + session_size);
				throw Root::Quota_exceeded();
			}

			Genode::Session_label label = Genode::label_from_args(args);
			Block::Policy const policy(args);

			/* ensure inclusive read and exclusive write sessions */
			for (Session_component *s = _sessions.first(); s; s = s->next()) {
				if (!(policy.writeable || s->writeable())) {
					/* both read-only, no problem */
					continue;
				} else if (((policy.offset <= s->offset()) && (policy.span   > s->offset())) ||
				           ((policy.offset >= s->offset()) && (policy.offset < s->span()))) {

					if (policy.writeable && s->writeable()) {
						Genode::error("write session '", label, "' would conflict "
						              "with write session '", s->label(), "'");
						throw Genode::Root::Unavailable();
					} else {
						Genode::warning("session '", label, "' will overlap "
						                "with session '", s->label(), "'");
					}
				}
			}

			Session_component *session =
				new (md_alloc()) Session_component(_driver_factory,
			                                       _ep, tx_buf_size,
				                                   policy, label);
			_sessions.insert(session);
			return session;
		}

		void _destroy_session(Session_component *session) override
		{
			_sessions.remove(session);
			destroy(md_alloc(), session);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep              entrypoint handling this root component
		 * \param md_alloc        allocator to allocate session components
		 * \param driver_factory  factory to create and destroy driver backend
		 * \param receiver        signal receiver managing signals of the client
		 */
		Root(Genode::Entrypoint &ep, Allocator *md_alloc,
		     Driver_factory &driver_factory)
		: Root_component(&ep.rpc_ep(), md_alloc),
		  _driver_factory(driver_factory), _ep(ep) { }
};

#endif /* _INCLUDE__BLOCK__COMPONENT_H_ */
