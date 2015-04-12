/*
 * \brief  9P protocol definitions
 * \athor  Emery Hemingway
 * \data   2015-03-20
 *
 * Adapted from plan9front/sys/include/fcall.h
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _9P_CLIENT__9P_H_
#define _9P_CLIENT__9P_H_

/* Genode includes */
#include <base/fixed_stdint.h>
#include <os/path.h>

using namespace Genode;


/* Use a different namespace so types can be redefined. */
namespace Plan9 {

	using namespace Genode;

	char VERSION9P[] = "9P2000";

	enum
	{
		ERRMAX	         = 128,  /* max length of error string */
		MAXWELEM         = 15,   /* This should be 16, but u9fs can only do 15. */
		DEFAULT_PORT     = 564,
		MAX_MESSAGE_SIZE = 8192,

		OREAD  = 0,
		OWRITE = 1,
		ORDWR  = 2,

		DMDIR  = 0x80000000
	};

	typedef uint32_t Fid;
	typedef uint32_t size_t;
	typedef uint8_t  type_t;
	typedef uint16_t tag_t;
	typedef uint8_t  mode_t;

	enum : tag_t { NOTAG = 0xFFFF };
	enum : Fid   { NOFID = 0XFFFFFFFF };

	/**
	 * Generate unique fid.
	 *
	 * 	All requests on a connection share the same fid space;
	 * 	when several clients share a connection, the agent
	 * 	managing the sharing must arrange that no two
	 * 	clients choose the same fid.
	 */
	Plan9::Fid unique_fid()
	{
		static Plan9::Fid count;
		return ++count;
	}

	/**
	 * Not in use; for local caching.
	  */
	struct Qid
	{
		uint64_t path;
		uint32_t vers;
		uint8_t  type;
	};

	struct Fcall
	{
		Fid    fid;
		tag_t  tag;
		type_t type;

		union {
			struct {
				Plan9::size_t  msize;   /* Tversion, Rversion */
				char          *version; /* Tversion, Rversion */
			};
			struct {
				tag_t   oldtag;  /* Tflush */
			};
			struct {
				char   *ename;   /* Rerror */
			};
			struct {
				uint32_t    afid;   /* Tauth, Tattach */
				char const *uname;  /* Tauth, Tattach */
				char const *aname;  /* Tauth, Tattach */
			};
			struct {
				uint32_t    perm;   /* Tcreate */
				char const *name;   /* Tcreate */
				uint8_t     mode;   /* Tcreate, Topen */
			};
			struct {
				Fid      newfid;          /* Twalk */
				uint16_t nwname;          /* Twalk */
				char    *wname[MAXWELEM]; /* Twalk */
			};
			struct {
				uint16_t nwqid;            /* Rwalk */
				Qid      wqid[MAXWELEM];   /* Rwalk */
			};
			struct {
				uint64_t offset;  /* Tread, Twrite */
				uint32_t count;   /* Tread, Twrite, Rread */
				char    *data;    /* Twrite, Rread */
			};
			struct {
				uint16_t  nstat;  /* Twstat, Rstat */
				char     *stat;   /* Twstat, Rstat */
			};
		};
	};

	#define get1(buffer, index, value) \
		value  = buffer[index++]; \

	#define get2(buffer, index, value) \
		value  = buffer[index++]; \
		value |= buffer[index++] << 8; \

	#define get4(buffer, index, value) \
		value  = buffer[index++]; \
		value |= buffer[index++] << 8; \
		value |= buffer[index++] << 16; \
		value |= buffer[index++] << 24; \

	#define get8(buffer, index, value) \
		value  = buffer[index++]; \
		value |= buffer[index++] << 8; \
		value |= buffer[index++] << 16; \
		value |= buffer[index++] << 24; \
		value |= buffer[index++] << 32; \
		value |= buffer[index++] << 40; \
		value |= buffer[index++] << 48; \
		value |= buffer[index++] << 56; \

	#define put1(buffer, index, value) \
		buffer[index++] = value & 0xFF; \

	#define put2(buffer, index, value) \
		buffer[index++] = value & 0xFF; \
		buffer[index++] = (value >> 8) & 0xFF; \

	#define put4(buffer, index, value) \
		buffer[index++] = value & 0xFF; \
		buffer[index++] = (value >> 8) & 0xFF; \
		buffer[index++] = (value >> 16) & 0xFF; \
		buffer[index++] = (value >> 24) & 0xFF; \

	#define put8(buffer, index, value) \
		buffer[index++] = value & 0xFF; \
		buffer[index++] = (value >> 8) & 0xFF; \
		buffer[index++] = (value >> 16) & 0xFF; \
		buffer[index++] = (value >> 24) & 0xFF; \
		buffer[index++] = (value >> 32) & 0xFF; \
		buffer[index++] = (value >> 40) & 0xFF; \
		buffer[index++] = (value >> 48) & 0xFF; \
		buffer[index++] = (value >> 56) & 0xFF; \

	enum
	{
		Tversion =	100,
		Rversion,
		Tauth =		102,
		Rauth,
		Tattach =	104,
		Rattach,
		Terror =	106,  /* illegal */
		Rerror,
		Tflush =	108,
		Rflush,
		Twalk =		110,
		Rwalk,
		Topen =		112,
		Ropen,
		Tcreate =	114,
		Rcreate,
		Tread =		116,
		Rread,
		Twrite =	118,
		Rwrite,
		Tclunk =	120,
		Rclunk,
		Tremove =	122,
		Rremove,
		Tstat =		124,
		Rstat,
		Twstat =	126,
		Rwstat,
		Tmax,
	};

	/**
	 * Convert a File_system mode to a Plan9 mode.
	 */
	Plan9::mode_t convert_mode(File_system::Mode mode)
	{
		switch (mode) {
			case File_system::STAT_ONLY:
			case File_system::READ_ONLY:  return Plan9::OREAD;
			case File_system::WRITE_ONLY: return Plan9::OWRITE;
			case File_system::READ_WRITE: return Plan9::ORDWR;
			default:         return Plan9::OREAD;
		}
	}

	enum { NULL_STAT_LEN = 2+2+4+13+4+4+4+8+2+2+2+2 };

	/**
	 * Set "dont't touch" values on the
	 * buffer space of a directory entry.
	 *
	 * \param buf  The start byte of a stat (the size[2] field).
	 */
	void zero_stat(uint8_t *buf)
	{
		int bi = 0;
		put2(buf, bi, (NULL_STAT_LEN-2));
		memset(&buf[2], 0xFF, 2+4+13+4+4+4+8);
		memset(&buf[2+2+4+13+4+4+4+8], 0x00, 2+2+2+2);
	}
}

#endif