/*
 * \brief  Argument type for denoting a network interface
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__NIC_TARGET_H_
#define _MODEL__NIC_TARGET_H_

#include "types.h"

namespace Sculpt { struct Nic_target; }


struct Sculpt::Nic_target : Noncopyable
{
	enum Policy { MANAGED, MANUAL } policy { MANAGED };

	bool manual()  const { return policy == MANUAL; }
	bool managed() const { return policy == MANAGED; }

	enum Type { OFF, LOCAL, WIRED, WIFI };

	/**
	 * Interactive selection by the user, used when managed policy is in effect
	 */
	Type managed_type { OFF };

	/**
	 * Selection by the manually-provided NIC-router configuration
	 */
	Type manual_type { OFF };

	/**
	 * Return currently active NIC target type
	 */
	Type type() const
	{
		/*
		 * Enforce the user's interactive choice to disable networking
		 * even if the NIC target is manually managed.
		 */
		if (managed_type == OFF)
			return OFF;

		return manual() ? manual_type : managed_type;
	}

	bool local() const { return type() == LOCAL; }
	bool wired() const { return type() == WIRED; }
	bool wifi()  const { return type() == WIFI; }

	bool nic_router_needed() const { return type() != OFF; }

	bool ready() const { return type() == WIRED || type() == WIFI; }
};

#endif /* _MODEL__NIC_TARGET_H_ */
