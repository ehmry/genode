/*
 * \brief  Proxy-ARP session utilities
 * \author Emery Hemingway
 * \date   2019-05-14
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SESSION_COMPONENT_H_
#define _SESSION_COMPONENT_H_

/* Genode includes */
#include <nic_session/nic_session.h>
#include <nic/packet_allocator.h>
#include <base/attached_ram_dataspace.h>
#include <base/heap.h>

namespace Net { class Session_resources; }

/**
 * Base class to manage session quotas and allocations
 */
class Net::Session_resources
{
	protected:

		Genode::Ram_quota_guard           _ram_guard;
		Genode::Cap_quota_guard           _cap_guard;
		Genode::Constrained_ram_allocator _ram_alloc;
		Genode::Attached_ram_dataspace    _tx_ds, _rx_ds;
		Genode::Heap                      _alloc;
		::Nic::Packet_allocator           _packet_alloc;

		Session_resources(Genode::Ram_allocator &ram,
		                  Genode::Region_map    &region_map,
		                  Genode::Ram_quota     ram_quota,
		                  Genode::Cap_quota     cap_quota,
		                  Nic::Session::Tx_size tx_size,
		                  Nic::Session::Rx_size rx_size)
		:
			_ram_guard(ram_quota), _cap_guard(cap_quota),
			_ram_alloc(ram, _ram_guard, _cap_guard),
			_tx_ds(_ram_alloc, region_map, tx_size.value),
			_rx_ds(_ram_alloc, region_map, rx_size.value),
			_alloc(_ram_alloc, region_map),
			_packet_alloc(&_alloc)
		{ }

	virtual ~Session_resources() { }
};

#endif
