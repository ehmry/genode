/*
 * \brief  RDRAND test
 * \author Emery Hemingway
 * \date   2019-02-05
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <os/rdrand.h>
#include <base/component.h>
#include <base/log.h>

using namespace Genode;

void Component::construct(Genode::Env &env)
{
	log("--- RDRAND test started ---");

	if (!Rdrand::supported()) {
		log("RDRAND instruction not supported");
		env.parent().exit(~0);
	}

	for (int i = 0; i < 8; i++)
		log("RDRAND ", (Hex)Rdrand::random64());

	log("--- RDRAND test finished ---");
	env.parent().exit(0);
}
