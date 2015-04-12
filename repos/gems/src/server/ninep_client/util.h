/*
 * \brief  Utilities
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include "9p.h"

#include <base/printf.h>


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


/**
 * Return true if character 'c' occurs in null-terminated string 'str'
 */
static inline bool string_contains(char const *str, char c)
{
	for (; *str; str++)
		if (*str == c)
			return true;
	return false;
}


/**
 * Return true if 'str' is a valid node name
 */
static inline bool valid_name(char const *str)
{
	if (string_contains(str, '/')) return false;

	/* must have at least one character */
	if (str[0] == 0) return false;

	return true;
}


/**
 * Return the length of the leftmost path element at `path',
 * that is the number of bytes before `/'.
 */
static inline uint16_t namelen(char const *path)
{
	uint16_t res = 0;
	for (; path && *path && *path != '/'; ++path, ++res);
	return res;
}

#endif /* _UTIL_H_ */
