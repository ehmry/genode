/*
 * \brief  Stdin to stdout echo test
 * \author Emery Hemingway
 * \date   2016-08-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Libc includes */
#include <unistd.h>
#include <stdio.h>

int main(void)
{
	int r;
	char buf[1024];

	printf("--- libc echo test ---\n");

	for (;;) {
		r = read(STDIN_FILENO, buf, sizeof(buf)-1);
		if (r < 0) return r;
		buf[r] = '\n';
		r = write(STDOUT_FILENO, buf, r+1);
		if (r < 0) return r;
	}

	return 0;
}
