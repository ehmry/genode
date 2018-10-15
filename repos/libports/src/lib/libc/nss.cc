/*
 * \brief  Name Service Switch dummies
 * \author Emery Hemingway
 * \date   2018-07-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

extern "C" {

#include <nsswitch.h>

int
_nsdispatch(void *retval, const ns_dtab disp_tab[], const char *database,
            const char *method_name, const ns_src defaults[], ...)
{
	Genode::warning("nsdispatch not implemented, "
	                "discarding ", database, " ", method_name, " request");
	return NS_SUCCESS;
}

} /* extern "C" */
