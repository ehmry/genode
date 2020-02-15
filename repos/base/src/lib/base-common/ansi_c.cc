/*
 * \brief  Implementation of basic C utilities
 * \author Emery Hemingway
 * \date   2020-01-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/string.h>

using namespace Genode;

extern "C" __attribute__((weak))
int memcmp(const void *p0, const void *p1, size_t size)
{
	return Genode::memcmp(p0, p1, size);
}

extern "C" __attribute__((weak))
size_t strlen(const char *s)
{
	return Genode::strlen(s);
}
