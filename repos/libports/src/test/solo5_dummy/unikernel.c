/*
 * \brief  Fallback unikernel binary for Solo5 runtime package
 * \author Emery Hemingway
 * \date   2019-02-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <solo5.h>

#define NSEC_PER_SEC 1000000000ULL

void _assert_fail(const char *x, const char *y, const char *z)
{
	solo5_abort();
}

int platform_puts(const char *buf, int n)
{
	solo5_console_write(buf, n);
	solo5_yield(solo5_clock_monotonic()+(NSEC_PER_SEC>>2));
	return n;
}

#include "bindings/lib.c"
#include "bindings/ee_printf.c"

void printfast(const char *buf)
{
	solo5_console_write(buf, strlen(buf));
}

int solo5_app_main(const struct solo5_start_info *si)
{
    printfast("            |      ___|");
    printfast("  __|  _ \\  |  _ \\ __ \\");
    printfast("\\__ \\ (   | | (   |  ) |");
    printfast("____/\\___/ _|\\___/____/");

	printf("  ");
	printf("This is a dummy unikernel provided through the depot, "
	       "to use this package correctly the 'unikernel.solo5' "
	       "ROM request must be routed to your binary.");

	printf("Command line: %s", si->cmdline);
	printf("Heap size: %ld MiB", si->heap_size >> 20);

	struct solo5_net_info ni;
	solo5_net_info(&ni);
	if (ni.mac_address[0]|ni.mac_address[1]|ni.mac_address[2]) {
		printf("MAC address: %x:%x:%x:%x:%x:%x",
		       ni.mac_address[0],
		       ni.mac_address[1],
		       ni.mac_address[2],
		       ni.mac_address[3],
		       ni.mac_address[4],
		       ni.mac_address[5]);
	}

	struct solo5_block_info bi;
	solo5_block_info(&bi);
	if (bi.capacity) {
		printf("Block device capacity: %ld MiB", bi.capacity >> 20);
	}

	/* Hack to sleep forever, this allows Sculpt to
	 * restart the unikernel when service routing changes.
	 */
	for (;;) solo5_yield(solo5_clock_monotonic() - 1);
	return SOLO5_EXIT_SUCCESS;
}
