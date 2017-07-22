/*---------------------------------------------------------------------------/
/  FatFs - FAT file system module configuration file
/---------------------------------------------------------------------------*/

#define _FFCONF 68300	/* Revision ID */

#define _FS_READONLY	0

#define _FS_MINIMIZE	0

#define _USE_STRFUNC	0

#define _USE_FIND		0

#define _USE_MKFS		0

#define _USE_FASTSEEK	0

#define _USE_EXPAND		0

#define _USE_CHMOD		0

#define _USE_LABEL		1

#define _USE_FORWARD	0

#define _CODE_PAGE	437

#define _USE_LFN	2
#define _MAX_LFN	255

#define _LFN_UNICODE	0

#define _STRF_ENCODE	3

#define _FS_RPATH	0

#define _VOLUMES	1

#define _STR_VOLUME_ID	0

#define _MULTI_PARTITION	0

#define _MIN_SS		512
#define _MAX_SS		4096

#define _USE_TRIM	0

#define _FS_NOFSINFO	0

#define _FS_TINY	0

#define _FS_EXFAT	1

#define _FS_NORTC	1
#define _NORTC_MON	1
#define _NORTC_MDAY	1
#define _NORTC_YEAR	2017

#define _FS_LOCK	0

#define _FS_REENTRANT	0
#define _FS_TIMEOUT		1000
#define _SYNC_t			HANDLE

/*--- End of configuration options ---*/
