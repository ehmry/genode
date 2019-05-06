/*
 * \brief  Media access control (MAC) address
 * \author Martin Stein
 * \date   2016-06-22
 */

/*
 * Copyright (C) 2016-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__MAC_ADDRESS_H_
#define _NET__MAC_ADDRESS_H_

/* OS includes */
#include <net/netaddress.h>

namespace Net { struct Mac_address; }

struct Net::Mac_address : Net::Network_address<6, ':', true>
{
	using Net::Network_address<6, ':', true>::Network_address;

	bool multicast() const { return addr[0] & 1; }

	Genode::uint64_t to_int() const
	{
		using Genode::uint64_t;

		/* EUI-48 to EUI-64 mapping */
		return
			(uint64_t)addr[0] << 56 |
			(uint64_t)addr[1] << 48 |
			(uint64_t)addr[2] << 40 |
			(uint64_t)0xff    << 32 |
			(uint64_t)0xff    << 24 |
			(uint64_t)addr[3] << 16 |
			(uint64_t)addr[4] <<  8 |
			(uint64_t)addr[5];
	}
};

#endif /* _NET__MAC_ADDRESS_H_ */
