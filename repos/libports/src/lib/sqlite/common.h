/*
 * \brief  Common SQLite VFS methods
 * \author Emery Hemingway
 * \date   2015-05-01
 *
 * In this file VFS refers to the SQLite VFS, not the Genode VFS.
 * See http://sqlite.org/vfs.html for more information.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/lock_guard.h>
#include <base/allocator_avl.h>
#include <file_system_session/connection.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>
#include <util/string.h>

/* jitterentropy includes */
namespace Jitter {
extern "C" {
#include <jitterentropy.h>
}
}


namespace Sqlite {


/* Use string operations without qualifier. */
using namespace Genode;


/* Sqlite includes */
extern "C" {
#include <sqlite3.h>
}


/**
 * Convert an Rtc::Timestamp to a Julian Day Number.
 *
 * \param   ts   Timestamp to convert.
 *
 * \return       Julian Day Number, rounded down.
 *
 * The Julian Day starts at noon and this function rounds down,
 * so the return value is effectively 12 hours behind.
 *
 * https://en.wikipedia.org/wiki/Julian_day#Calculation
 */
unsigned julian_day(Rtc::Timestamp ts)
{
	unsigned a = (14 - ts.month) / 12;
	unsigned y = ts.year + 4800 - a;
	unsigned m = ts.month + 12*a - 3;

	return ts.day + (153*m + 2)/5 + 365*y + y/4 - y/100 + y/400 - 32046;
}


#define NOT_IMPLEMENTED PWRN("Sqlite::%s not implemented", __func__);

static Timer::Connection _timer;
static Jitter::rand_data *_jitter;


/**
 * Return base-name portion of null-terminated path string
 */
static inline char const *basename(char const *path)
{
	char const *start = path;

	for (; *path; path++)
		if (*path == '/')
			start = path + 1;

	return start;
}


static int genode_randomness(sqlite3_vfs *pVfs, int len, char *buf)
{
	static Genode::Lock lock;
	Genode::Lock::Guard guard(lock);

	return Jitter::jent_read_entropy(_jitter, buf, len);
}


static int genode_lock(sqlite3_file *pFile, int eLock)
{
	NOT_IMPLEMENTED
	return SQLITE_OK;
}
static int genode_unlock(sqlite3_file *pFile, int eLock)
{
	NOT_IMPLEMENTED
	return SQLITE_OK;
}
static int genode_check_reserved_lock(sqlite3_file *pFile, int *pResOut)
{
	NOT_IMPLEMENTED
	*pResOut = 0;
	return SQLITE_OK;
}


/*
 * No xFileControl() verbs are implemented by this VFS.
 * Without greater control over writing, there isn't utility in processing this.
 * See https://www.sqlite.org/c3ref/c_fcntl_busyhandler.html
*/
static int genode_file_control(sqlite3_file *pFile, int op, void *pArg) { return SQLITE_OK; }


/*
 * The xSectorSize() and xDeviceCharacteristics() methods. These two
 * may return special values allowing SQLite to optimize file-system
 * access to some extent. But it is also safe to simply return 0.
 */
static int genode_sector_size(sqlite3_file *pFile) { return 0; }


static int genode_device_characteristics(sqlite3_file *pFile) { return 0; }


static int random_string(char *buf, int len)
{
	const unsigned char chars[] =
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789";

	if (genode_randomness(0, len, buf)  != (len))
		return -1;

	for(int i=0; i <len; i++) {
		buf[i] = (char)chars[ ((unsigned char)buf[i]) % (sizeof(chars)-1) ];
	}
	return 0;
}


static int genode_full_pathname(sqlite3_vfs *pVfs, const char *path_in, int out_len, char *path_out)
{
	/*
	 * No support for current working directory, work from /.
	 */
	if (path_in[0]=='/')
		strncpy(path_out, path_in, out_len);
	else {
		path_out[0] = '/';
		strncpy(path_out+1, path_in, out_len-1);
	}

	path_out[out_len-1] = '\0';
	return SQLITE_OK;
}


/*
 * The following four VFS methods:
 *
 *   xDlOpen
 *   xDlError
 *   xDlSym
 *   xDlClose
 *
 * are supposed to implement the functionality needed by SQLite to load
 * extensions compiled as shared objects. This simple VFS does not support
 * this functionality, so the following functions are no-ops.
 */
static void *genode_dl_open(sqlite3_vfs *pVfs, const char *zPath)
{
	NOT_IMPLEMENTED
	return 0;
}
static void genode_dl_error(sqlite3_vfs *pVfs, int nByte, char *zErrMsg)
{
	NOT_IMPLEMENTED
	sqlite3_snprintf(nByte, zErrMsg, "Loadable extensions are not implemented");
	zErrMsg[nByte-1] = '\0';
}
static void (*genode_dl_sym(sqlite3_vfs *pVfs, void *pH, const char *z))(void)
{
	NOT_IMPLEMENTED
	return 0;
}
static void genode_dl_close(sqlite3_vfs *pVfs, void *pHandle)
{
	NOT_IMPLEMENTED
	return;
}


/*
 * Sleep for at least nMicro microseconds. Return the (approximate) number
 * of microseconds slept for.
 */
static int genode_sleep(sqlite3_vfs *pVfs, int nMicro)
{
	unsigned long then, now;

	then = _timer.elapsed_ms();
	_timer.usleep(nMicro);
	now = _timer.elapsed_ms();

	return (now - then) / 1000;
}


/*
 * Write into *pTime the current time and date as a Julian Day
 * number times 86400000. In other words, write into *piTime
 * the number of milliseconds since the Julian epoch of noon in
 * Greenwich on November 24, 4714 B.C according to the proleptic
 * Gregorian calendar.
 *
 * JULIAN DATE IS NOT THE SAME AS THE JULIAN CALENDAR,
 * NOR WAS IT DEVISED BY SOMEONE NAMED JULIAN.
 */
static int genode_current_time(sqlite3_vfs *pVfs, double *pTime)
{
	try {
		Rtc::Connection rtc;
		Rtc::Timestamp ts = rtc.current_time();

		*pTime = julian_day(ts)  * 86400000.0;
		*pTime += (ts.hour + 12) *   360000.0;
		*pTime += ts.minute      *    60000.0;
		*pTime += ts.second      *     1000.0;
		*pTime += ts.microsecond /     1000.0;
	} catch (...) {
		PWRN("RTC not present, using dummy time");
		*pTime = _timer.elapsed_ms();
	}

	return SQLITE_OK;
}


/* See above. */
static int genode_current_time_int64(sqlite3_vfs *pVfs, sqlite3_int64 *pTime)
{
	try {
		Rtc::Connection rtc;
		Rtc::Timestamp ts = rtc.current_time();

		*pTime = julian_day(ts)  * 86400000;
		*pTime += (ts.hour + 12) *   360000;
		*pTime += ts.minute      *    60000;
		*pTime += ts.second      *     1000;
		*pTime += ts.microsecond /     1000;
	} catch (...) {
		PWRN("RTC not present, using dummy time");
		*pTime = _timer.elapsed_ms();
	}

	return SQLITE_OK;
}


} /* namespace Sqlite */
