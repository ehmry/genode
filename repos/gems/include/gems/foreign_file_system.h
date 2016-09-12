/*
 * \brief  C interface to a File_system rpc server
 * \author Emery Hemingway
 * \date   2016-09-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GEMS__FOREIGN_FILE_SYSTEM_H_
#define _GEMS__FOREIGN_FILE_SYSTEM_H_

#include <base/fixed_stdint.h>

#ifdef __cplusplus
namespace File_system {
extern "C" {
#endif


typedef genode_uint64_t Genode_fs_size_t;

enum Genode_fs_err { /* Shout, shout, let it all out */
	INVALID_HANDLE,
	INVALID_NAME,
	LOOKUP_FAILED,
	NAME_TOO_LONG,
	NODE_ALREADY_EXISTS,
	NO_SPACE,
	NOT_EMPTY,
	OUT_OF_METADATA,
	PERMISSION_DENIED
};

struct Genode_fs_status {
	genode_uint64_t size;
	unsigned        mode;
	unsigned long   inode;
};


/**
 * Open or create a file and return a new handle.
 */
int genode_fs_file(int directory, char const *name, unsigned mode, unsigned create, int *err);


/**
 * Open or create a symlink
 */
int genode_fs_symlink(int directory, char const *path, unsigned create, int *err);


/**
 * Open or create a directory
 */
int genode_fs_directory(char const *path, unsigned create, int *err);


/**
 * Open an existing node
 */
int genode_fs_node(char const *path, int *err);


/**
 * Close an open node
 */
void genode_fs_close(int node);


/**
 * Request information about an open node
 */
void genode_fs_status(int node, Genode_fs_status *status);


/**
 * Delete a directory entry
 *
 * Returns 0 on success, -1 on failure
 */
int genode_fs_unlink(int directory, char const *name, int *err);


/**
 * Truncate or expand a file
 *
 * Returns 0 on success, -1 on failure
 */
int genode_fs_resize(int file, Genode_fs_size_t size, int *err);


/**
 * Rename or move directory entry
 *
 * Returns 0 on success, -1 on failure
 */
int genode_fs_move(int from_dir, char const *from_name,
                   int   to_dir, char const   *to_name, int *err);


/**
 * Synchronize node
 */
void genode_fs_sync(int node);


/**
 * Read a node
 */
unsigned genode_fs_read(int node, char *dst, unsigned len);


/**
 * Write a node
 */
unsigned genode_fs_write(int node, char const *src, unsigned len);


/**
 * Initialize and announce File_system RPC service
 */
void genode_fs_init(void *env);


#ifdef __cplusplus
} }
#endif

#endif /* _GEMS__FOREIGN_FILE_SYSTEM_H_ */