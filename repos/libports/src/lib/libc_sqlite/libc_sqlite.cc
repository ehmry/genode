/*
 * \brief  SQLite VFS layer for Genode
 * \author Emery Hemingway
 * \date   2015-01-31
 *
 * In this file VFS refers to the SQLite VFS, not the Genode VFS.
 * See http://sqlite.org/vfs.html for more information.
 *
 * Filesystem calls wrap our libC, clock and timer calls use native
 * service sessions, and there is no file locking support.
 *
 * The filesystem interface is unoptimized and just simple
 * enough to meet the minimum requirements for a legacy VFS.
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
#include <file_system_session/file_system_session.h>

/* Local includes */
#include <common.h>


namespace Sqlite {

extern "C" {
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
}


/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type Genode_file.
*/
typedef struct Genode_file Genode_file;
struct Genode_file {
	sqlite3_file base;  /* Base class. Must be first. */
	int fd;             /* File descriptor */
	char *delete_path;  /* Delete this path on close */
};


static int genode_delete(sqlite3_vfs *pVfs, const char *pathname, int dirSync)
{
	int rc;

	rc = unlink(pathname);
	if (rc !=0 && errno==ENOENT)
		 return SQLITE_OK;

	if (rc == 0 && dirSync) {
		int dfd;
		int i;
		char dir[File_system::MAX_PATH_LEN];

		sqlite3_snprintf(File_system::MAX_PATH_LEN, dir, "%s", pathname);
		dir[File_system::MAX_PATH_LEN-1] = '\0';
		for (i = strlen(dir); i > 1 && dir[i] != '/'; i--)
			dir[i] = '\0';

		dfd = open(dir, O_RDONLY);
	if (dfd < 0) {
			rc = -1;
		} else {
			rc = fsync(dfd);
			close(dfd);
		}
	}

	return rc ?
		SQLITE_IOERR_DELETE : SQLITE_OK;
}


static int genode_close(sqlite3_file *pFile)
{
	int rc;
	Genode_file *p = (Genode_file*)pFile;

	rc = close(p->fd);
	if (rc)
		return SQLITE_IOERR_CLOSE;

	if (p->delete_path) {
		rc = genode_delete(NULL, p->delete_path, false);
		if (rc == SQLITE_OK) {
			sqlite3_free(p->delete_path);
		} else {
			return rc;
		}
	}

	return SQLITE_OK;
}


static int genode_write(sqlite3_file *pFile, const void *buf, int count, sqlite_int64 offset)
{
	// TODO track the offset in pFile?
	Genode_file *p = (Genode_file*)pFile;

	if (lseek(p->fd, offset, SEEK_SET) != offset)
		return SQLITE_IOERR_SEEK;

	if (write(p->fd, buf, count) != count)
		return SQLITE_IOERR_WRITE;

	return SQLITE_OK;
}


static int genode_read(sqlite3_file *pFile, void *buf, int count, sqlite_int64 offset)
{
	// TODO track the offset in pFile?
	Genode_file *p = (Genode_file*)pFile;

	if (lseek(p->fd, offset, SEEK_SET) != offset) {
		return SQLITE_IOERR_SEEK;
	}

	int n = read(p->fd, buf, count);
	if (n != count) {
		/* Unread parts of the buffer must be zero-filled */
		memset(&((char*)buf)[n], 0, count-n);
		return SQLITE_IOERR_SHORT_READ;
	}

	return SQLITE_OK;
}


static int genode_truncate(sqlite3_file *pFile, sqlite_int64 size)
{
	Genode_file *p = (Genode_file*)pFile;

	return (ftruncate(p->fd, size)) ?
		SQLITE_IOERR_FSYNC : SQLITE_OK;
}


static int genode_sync(sqlite3_file *pFile, int flags)
{
	Genode_file *p = (Genode_file*)pFile;

	return (fsync(p->fd)) ?
		SQLITE_IOERR_FSYNC : SQLITE_OK;
}


static int genode_file_size(sqlite3_file *pFile, sqlite_int64 *pSize)
{
	Genode_file *p = (Genode_file*)pFile;
	struct stat s;

	if (fstat(p->fd, &s) != 0)
		return SQLITE_IOERR_FSTAT;

	*pSize = s.st_size;

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

	if (!name) {
		#define TEMP_PREFIX "sqlite_"
		#define TEMP_LEN 24

		char *temp = (char*)sqlite3_malloc(TEMP_LEN);

		strcpy(temp, TEMP_PREFIX);
		if (random_string(&temp[sizeof(TEMP_PREFIX)-1], TEMP_LEN-(sizeof(TEMP_PREFIX)))) {
			sqlite3_free(temp);
			return SQLITE_ERROR;
		}
		temp[TEMP_LEN-1] = '\0';

		name = temp;
		p->delete_path = temp;
	}

	int oflags = 0;
	if( flags&SQLITE_OPEN_EXCLUSIVE ) oflags |= O_EXCL;
	if( flags&SQLITE_OPEN_CREATE )    oflags |= O_CREAT;
	if( flags&SQLITE_OPEN_READONLY )  oflags |= O_RDONLY;
	if( flags&SQLITE_OPEN_READWRITE ) oflags |= O_RDWR;

	p->fd = open(name, oflags);
	if (p->fd <0)
		return SQLITE_CANTOPEN;

	if (pOutFlags)
		*pOutFlags = flags;

	p->base.pMethods = &genodeio;
	return SQLITE_OK;
}


static int genode_access(sqlite3_vfs *pVfs, const char *path, int flags, int *pResOut)
{
	int mode;
	
	switch (flags) {
	case SQLITE_ACCESS_EXISTS:
		mode = F_OK;
		break;
	case SQLITE_ACCESS_READWRITE:
		mode = R_OK|W_OK;
		break;
	case SQLITE_ACCESS_READ:
		mode = R_OK;
		break;
	default:
		return SQLITE_INTERNAL;
	}

	*pResOut = (access(path, mode) == 0);
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

	static sqlite3_vfs genode_vfs = {
		2,                         /* iVersion */
		sizeof(Genode_file),       /* szOsFile */
		File_system::MAX_PATH_LEN, /* mxPathname */
		NULL,                      /* pNext */
		VFS_NAME,                  /* zName */
		NULL,                      /* pAppData */
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
	Jitter::jent_entropy_collector_free(_jitter);

	return SQLITE_OK;
}

} /* namespace Sqlite */
