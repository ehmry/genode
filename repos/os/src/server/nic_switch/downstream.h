/*
 * \brief  Downstream
 * \author Emery Hemingway
 * \date   2019-04-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DOWNSTREAM_H_
#define _DOWNSTREAM_H_

/* Genode includes */
#include <nic/component.h>
#include <base/heap.h>

namespace Nic_switch {
	struct Tx_size { Genode::size_t value; };
	struct Rx_size { Genode::size_t value; };

	class Session_resources;
	class Session_component;
}

/**
 * Base class to manage session quotas and allocations
 */
class Nic_switch::Session_resources
{
