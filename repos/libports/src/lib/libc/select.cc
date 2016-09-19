/*
 * \brief  select() implementation
 * \author Christian Prochaska
 * \date   2010-01-21
 *
 * the 'select()' implementation is partially based on the lwip version as
 * implemented in 'src/api/sockets.c'
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <os/timed_semaphore.h>

#include <libc-plugin/plugin_registry.h>
#include <libc-plugin/plugin.h>

#include <sys/select.h>
#include <signal.h>

using namespace Libc;


/** Description for a task waiting in select */
struct libc_select_cb
{
	struct libc_select_cb *next;
	int nfds;
	int nready;
	fd_set readset;
	fd_set writeset;
	fd_set exceptset;
	/** don't signal the same semaphore twice: set to 1 when signalled */
	int sem_signalled;
	/** semaphore to wake up a task waiting for select */
	Timed_semaphore *sem;
};


extern "C" int
__attribute__((weak))
_select(int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, struct timeval *timeout)
{
	int nready = 0;
	for (Plugin *plugin = plugin_registry()->first(); plugin; plugin = plugin->next())
		if (plugin->supports_select(nfds, readfds, writefds, exceptfds, timeout))
			nready += plugin->select(nfds, readfds, writefds, exceptfds, timeout);
	return nready;
}


extern "C" int
__attribute__((weak))
select(int nfds, fd_set *readfds, fd_set *writefds,
       fd_set *exceptfds, struct timeval *timeout)
{
	return _select(nfds, readfds, writefds, exceptfds, timeout);
}


extern "C" int
__attribute__((weak))
_pselect(int nfds, fd_set *readfds, fd_set *writefds,
         fd_set *exceptfds, const struct timespec *timeout,
         const sigset_t *sigmask)
{
	struct timeval tv;
	sigset_t origmask;
	int nready;

	if (timeout) {
		tv.tv_usec = timeout->tv_nsec / 1000;
		tv.tv_sec = timeout->tv_sec;
	}

	if (sigmask)
		sigprocmask(SIG_SETMASK, sigmask, &origmask);
	nready = select(nfds, readfds, writefds, exceptfds, &tv);
	if (sigmask)
		sigprocmask(SIG_SETMASK, &origmask, NULL);

	return nready;
}


extern "C" int
__attribute__((weak))
pselect(int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, const struct timespec *timeout,
        const sigset_t *sigmask)
{
	return _pselect(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}
