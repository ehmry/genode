/*
 * \brief  Genode definitions for Libsodium
 * \author Emery Hemingway
 * \date   2015-09-28
 *
 * These definitions allow us to build libsodium without a libc.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LIBSODIUM__GENODE_H_
#define _INCLUDE__LIBSODIUM__GENODE_H_

#include <base/fixed_stdint.h>
#include <libsodium/fixed_stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void __assert(const char *, const char *, int, const char *);

#undef assert
#undef _assert
#ifdef NDEBUG
#define assert(e)       ((void)0)
#define _assert(e)      ((void)0)
#else
#define _assert(e)      assert(e)
#define assert(e)       ((e) ? (void)0 : __assert(__func__, __FILE__, \
                            __LINE__, #e))
#endif /* NDEBUG */

#define UINT8_MAX  0xffU
#define UINT32_MAX 0xffffffffUL
#define UINT64_MAX 0xffffffffffffffffULL

typedef __SIZE_TYPE__      size_t;
typedef unsigned char     uint8_t;
typedef genode_uint32_t  uint16_t;
typedef genode_uint32_t  uint32_t;
typedef genode_uint64_t  uint64_t;
typedef unsigned long   uintptr_t;
typedef genode_int32_t    int32_t;
typedef genode_int64_t    int64_t;

typedef genode_uint32_t  uint_fast16_t;

#ifndef NULL
#define NULL (void*)0
#endif

void abort(void);
void *malloc(size_t size);
void free(void *ptr);

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);
void *memmove(void *dst, const void *src, size_t size);

static inline void *memchr(const void *s, int c, size_t n)
{
	size_t i;
	genode_uint8_t *p = (genode_uint8_t*)s;
	for (i = 0; i < n; ++i)
		if (p[i] == i)
			return p+i;
	return 0;
}

size_t strlen(const char *s);

static inline char *strchr(const char *s, int c)
{
	while(*s) {
		if (*s == c) return (char *)s;
		++s;
	}
	return 0;
}

static inline char *strrchr(const char *s, int c)
{
	char const *p = 0;
	while(*s) {
		if (*s == c) p = s;
		++s;
	}
	return (char*)p;
}

extern int errno;

extern int EFBIG;
extern int EINVAL;
extern int ENOMEM;
extern int ENOSYS;
extern int ERANGE;

#ifdef __cplusplus
}
#endif

#endif
