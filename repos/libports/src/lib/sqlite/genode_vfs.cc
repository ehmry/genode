/*
 * \brief  SQLite VFS layer for Genode
 * \author Emery Hemingway
 * \date   2015-01-31
 *
 * In this file VFS refers to the SQLite VFS, not the Genode VFS.
 * See http://sqlite.org/vfs.html for more information.
 *
 * This implementation of sqlite3_io_methods only meets the first
 * version of that interface. Implementing the shared memory I/O
 * methods of the second version should increase performance.
 * https://www.sqlite.org/c3ref/io_methods.html
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <base/lock_guard.h>
#include <base/allocator_avl.h>
#include <file_system_session/connection.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>

/* jitterentropy includes */
namespace Jitter {
extern "C" {
#include <jitterentropy.h>
}
}


namespace Sqlite {


/* Sqlite includes */
extern "C" {
#include <sqlite3.h>
}

extern "C" {
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
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


struct Fs_state
{
	Genode::Allocator_avl _tx_block_alloc;
	File_system::Connection fs;

	/**
	 * Constructor
	 */
	Fs_state()
	:
		_tx_block_alloc(Genode::env()->heap()),
		fs(_tx_block_alloc)
	{ }

	File_system::Dir_handle dir_of(const char *path, bool create = false)
	{
		int i;
		for (i = strlen(path); i != 0; --i)
			if (path[i] == '/')
				break;

		if (i) {
			char s[++i];
			memcpy(s, path, i-1);
			s[i] = '\0';
			return fs.dir(s, create);
		}
		return fs.dir("/", create);
	}
};


/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type Genode_file.
*/
typedef struct Genode_file Genode_file;
struct Genode_file {
	sqlite3_file base;  /* Base class. Must be first. */
	Fs_state *st;
	File_system::File_handle fh;
	char delete_path[File_system::MAX_PATH_LEN];  /* Delete this path on close */
};


static int genode_randomness(sqlite3_vfs *pVfs, int len, char *buf)
{
	static Genode::Lock lock;
	Genode::Lock::Guard guard(lock);

	return Jitter::jent_read_entropy(_jitter, buf, len);
}


static int genode_delete(sqlite3_vfs *pVfs, const char *pathname, int dirSync)
{
	Fs_state *st = (Fs_state*)pVfs->pAppData;
	try {
		File_system::Dir_handle dh = st->dir_of(pathname);
		st->fs.unlink(dh, basename(pathname));
	}
	catch (...) { return SQLITE_IOERR_DELETE; }
	return SQLITE_OK;
}


static int genode_close(sqlite3_file *pFile)
{
	Genode_file *p = (Genode_file*)pFile;

	try { p->st->fs.close(p->fh); }
	catch (...) { return SQLITE_IOERR_CLOSE; }

	if (strlen(p->delete_path)) {
		try {
			File_system::Dir_handle dh = p->st->dir_of(&p->delete_path[0]);
			p->st->fs.unlink(dh, basename(&p->delete_path[0]));
		}
		catch (File_system::Lookup_failed) {
			return SQLITE_IOERR_DELETE_NOENT;
		}
		catch (...) {
			return SQLITE_IOERR_DELETE;
		}
	}

	return SQLITE_OK;
}


/*
 * Reads and writes assume that SQLite will not operate
 * over more than 4096 bytes at a time, and likely only 512.
 * https://www.sqlite.org/atomiccommit.html#hardware
 *
 * These next two methods are not thread safe.
 */


static int genode_write(sqlite3_file *pFile, const void *buf, int count, sqlite_int64 offset)
{
	int rc = SQLITE_OK;
	Genode_file *p = (Genode_file*)pFile;

	File_system::Session::Tx::Source &source = *p->st->fs.tx();
	File_system::Packet_descriptor
		packet(source.alloc_packet(count),
		       0, p->fh,
		       File_system::Packet_descriptor::WRITE,
		       count, offset);

	memcpy(source.packet_content(packet), buf, count);

	try {
		source.submit_packet(packet);
		source.get_acked_packet();
	}
	catch (...) {
		rc = SQLITE_IOERR_WRITE;
	}

	source.release_packet(packet);
	return rc;
}


static int genode_read(sqlite3_file *pFile, void *buf, int count, sqlite_int64 offset)
{
	int rc = SQLITE_OK;
	Genode_file *p = (Genode_file*)pFile;

	File_system::Session::Tx::Source &source = *p->st->fs.tx();
	File_system::Packet_descriptor packet_in, packet_out;

	packet_in = File_system::Packet_descriptor(source.alloc_packet(count), 0, p->fh,
	                                           File_system::Packet_descriptor::READ,
	                                           count, offset);

	try {
		source.submit_packet(packet_in);

		packet_out = source.get_acked_packet();

		int actual = packet_out.length();
		memcpy(buf, source.packet_content(packet_out), actual);

		if ((actual < count)) {
			/* Unread parts of the buffer must be zero-filled */
			memset(&((char*)buf)[actual], 0, count-actual);
			rc = SQLITE_IOERR_SHORT_READ;
		}
	}
	catch (...) { rc = SQLITE_IOERR_READ; }

	source.release_packet(packet_out);
	return rc;
};


static int genode_truncate(sqlite3_file *pFile, sqlite_int64 size)
{
	Genode_file *p = (Genode_file*)pFile;

	try { p->st->fs.truncate(p->fh, size); }
	catch (...) { return SQLITE_IOERR_TRUNCATE; }

	return SQLITE_OK;
}


static int genode_sync(sqlite3_file *pFile, int flags)
{
	Genode_file *p = (Genode_file*)pFile;

	/* Should sync at the file, but this is the best we can do for now. */
	try { p->st->fs.sync(); }
	catch (...) { return SQLITE_IOERR_FSYNC; }
	return SQLITE_OK;
}


static int genode_file_size(sqlite3_file *pFile, sqlite_int64 *pSize)
{
	Genode_file *p = (Genode_file*)pFile;

	try {
		File_system::Status s = p->st->fs.status(p->fh);
		*pSize = s.size;
	}
	catch (...) { return SQLITE_IOERR_FSTAT; }

	return SQLITE_OK;
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

	if (genode_randomness(NULL, len, buf)  != (len))
		return -1;

	for(int i=0; i <len; i++) {
		buf[i] = (char)chars[ ((unsigned char)buf[i]) % (sizeof(chars)-1) ];
	}
	return 0;
}


static int genode_open(
		sqlite3_vfs *pVfs,
		const char *name,               /* File to open, or 0 for a temp file */
		sqlite3_file *pFile,            /* Pointer to Genode_file struct to populate */
		int flags,                      /* Input SQLITE_OPEN_XXX flags */
		int *pOutFlags                  /* Output SQLITE_OPEN_XXX flags (or NULL) */
	)
{
	static const sqlite3_io_methods genodeio = {
		1,                               /* iVersion */
		genode_close,                    /* xClose */
		genode_read,                     /* xRead */
		genode_write,                    /* xWrite */
		genode_truncate,                 /* xTruncate */
		genode_sync,                     /* xSync */
		genode_file_size,                /* xFileSize */
		genode_lock,                     /* xLock */
		genode_unlock,                   /* xUnlock */
		genode_check_reserved_lock,      /* xCheckReservedLock */
		genode_file_control,             /* xFileControl */
		genode_sector_size,              /* xSectorSize */
		genode_device_characteristics    /* xDeviceCharacteristics */
	};

	Genode_file *p = (Genode_file*)pFile;
	memset(p, 0, sizeof(Genode_file));
	p->st = (Fs_state*)pVfs->pAppData;

	File_system::Mode mode;
	if(flags&SQLITE_OPEN_READONLY)
		mode = File_system::READ_ONLY;
	if (flags&SQLITE_OPEN_READWRITE)
		mode = File_system::READ_WRITE;
	bool create = flags&SQLITE_OPEN_CREATE;

	if (!name) {
		#define TEMP_PREFIX "sqlite_"
		#define TEMP_LEN 24

		char *s = &p->delete_path[0];
		strcpy(s, TEMP_PREFIX);

		if (random_string(s + sizeof(TEMP_PREFIX)-1, TEMP_LEN-(sizeof(TEMP_PREFIX)))) {
			return SQLITE_ERROR;
		}
		p->delete_path[TEMP_LEN-1] = '\0';

		name = (const char*)s;
		create = true;
	} else if (flags&SQLITE_OPEN_DELETEONCLOSE)
		strncpy(&p->delete_path[0], name, File_system::MAX_PATH_LEN);

	try {
		File_system::Dir_handle dh = p->st->dir_of(name, false);
		p->fh = p->st->fs.file(dh, basename(name), mode, create);
	}
	catch (...) { return SQLITE_CANTOPEN; }

	/* Ignore flags. */
	if (pOutFlags)
		*pOutFlags = flags
			& (SQLITE_OPEN_DELETEONCLOSE |
		       SQLITE_OPEN_READWRITE   |
		       SQLITE_OPEN_READONLY  |
		       SQLITE_OPEN_CREATE);

	p->base.pMethods = &genodeio;
	return SQLITE_OK;
}


static int genode_access(sqlite3_vfs *pVfs, const char *path, int flags, int *pResOut)
{
	using namespace File_system;
	Fs_state *st = (Fs_state*)pVfs->pAppData;

	File_system::Dir_handle dh;
	File_system::File_handle fh;
	File_system::Mode mode;
	switch (flags) {
	case SQLITE_ACCESS_EXISTS:    mode = STAT_ONLY;  break;
	case SQLITE_ACCESS_READWRITE: mode = READ_WRITE; break;
	case SQLITE_ACCESS_READ:      mode = READ_ONLY;  break;
	}
	try {
		dh = st->dir_of(path);
		fh = st->fs.file(dh, basename(path), mode, false);
		*pResOut = true;
	}
	catch (...) {
		st->fs.close(fh);
		st->fs.close(dh);
		*pResOut = false;
	}
	return SQLITE_OK;
}


static int genode_full_pathname(sqlite3_vfs *pVfs, const char *path_in, int out_len, char *path_out)
{
	char dir[File_system::MAX_PATH_LEN];
	if (path_in[0]=='/') {
		strncpy(path_out, path_in, out_len);
	} else {
		if (getcwd(dir, sizeof(dir)) == 0)
			return SQLITE_IOERR;
		if (strcmp(dir, "/") == 0)
			sqlite3_snprintf(out_len, path_out, "/%s", path_in);
		else
			sqlite3_snprintf(out_len, path_out, "%s/%s", dir, path_in);
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


/*****************************************
  ** Library initialization and cleanup **
  ****************************************/

#define VFS_NAME "genode"

int sqlite3_os_init(void)
{
	int ret = Jitter::jent_entropy_init();
	if(ret) {
		PERR("Jitter entropy initialization failed with error code %d", ret);
		return SQLITE_ERROR;
	}

	_jitter = Jitter::jent_entropy_collector_alloc(0, 0);
	if (!_jitter) {
		PERR("Jitter entropy collector initialization failed");
		return SQLITE_ERROR;
	}

	Fs_state *st = new (Genode::env()->heap()) Fs_state();

	static sqlite3_vfs genode_vfs = {
		2,                         /* iVersion */
		sizeof(Genode_file),       /* szOsFile */
		File_system::MAX_PATH_LEN, /* mxPathname */
		NULL,                      /* pNext */
		VFS_NAME,                  /* zName */
		st,                        /* pAppData */
		genode_open,               /* xOpen */
		genode_delete,             /* xDelete */
		genode_access,             /* xAccess */
		genode_full_pathname,      /* xFullPathname */
		genode_dl_open,            /* xDlOpen */
		genode_dl_error,           /* xDlError */
		genode_dl_sym,             /* xDlSym */
		genode_dl_close,           /* xDlClose */
		genode_randomness,         /* xRandomness */
		genode_sleep,              /* xSleep */
		genode_current_time,       /* xCurrentTime */
		NULL,                      /* xGetLastError */
		genode_current_time_int64, /* xCurrentTimeInt64 */
	};

	sqlite3_vfs_register(&genode_vfs, 0);
	return SQLITE_OK;
}


int sqlite3_os_end(void)
{
	sqlite3_vfs *vfs = sqlite3_vfs_find(VFS_NAME);
	sqlite3_vfs_unregister(vfs);
	destroy(Genode::env()->heap(), (Fs_state*)vfs->pAppData);
	Jitter::jent_entropy_collector_free(_jitter);

	return SQLITE_OK;
}

} /* namespace Sqlite */
