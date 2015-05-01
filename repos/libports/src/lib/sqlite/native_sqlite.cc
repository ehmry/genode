/*
 * \brief  SQLite VFS with native file system sessions
 * \author Emery Hemingway
 * \date   2015-05-01
 *
 * In this file VFS refers to the SQLite VFS, not the Genode VFS.
 * See http://sqlite.org/vfs.html for more information.
 *
 * This implementation of sqlite3_io_methods only meets the first
 * version of that interface. Implementing the shared memory I/O
 * methods of the second version should increase performance.
 * https://www.sqlite.org/c3ref/io_methods.html
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/allocator_avl.h>
#include <file_system_session/connection.h>

/* Local includes */
#include "common.h"

namespace Sqlite {


/* Use string operations without qualifier. */
using namespace Genode;

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
		/* SQLite likes the number 512. */
		fs(_tx_block_alloc, 1024)
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


static int genode_delete(sqlite3_vfs *pVfs, const char *pathname, int dirSync)
{
	Fs_state *st = (Fs_state*)pVfs->pAppData;
	try {
		File_system::Dir_handle dh = st->dir_of(pathname);
		st->fs.unlink(dh, basename(pathname));
	}
	catch (File_system::Lookup_failed) {
		return SQLITE_IOERR_DELETE_NOENT;
	}
	catch (...) {
		return SQLITE_IOERR_DELETE;
	}
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
		catch (File_system::Lookup_failed) { }
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
	// TODO: locking
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
	catch (...) { rc = SQLITE_IOERR_WRITE; }

	source.release_packet(packet);
	return rc;
}


static int genode_read(sqlite3_file *pFile, void *buf, int count, sqlite_int64 offset)
{
	// TODO: locking
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
	catch (...) {
		PERR("SQLite::%s failed", __func__);
		return SQLITE_IOERR_FSYNC;
	}
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
		strncpy(s, TEMP_PREFIX, sizeof(TEMP_PREFIX));

		if (random_string(s + sizeof(TEMP_PREFIX)-1, TEMP_LEN-(sizeof(TEMP_PREFIX))))
			return SQLITE_ERROR;

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
	File_system::Mode mode = STAT_ONLY;
	switch (flags) {
	case SQLITE_ACCESS_EXISTS:    break;
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

/*****************************************
  ** Library initialization and cleanup **
  ****************************************/

#define VFS_NAME "genode_native"

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
		0,                      /* pNext */
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
		0,                      /* xGetLastError */
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
