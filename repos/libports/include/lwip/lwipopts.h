/*
 * \brief  Configuration file for LwIP, adapt it to your needs.
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2009-11-10
 *
 * See lwip/src/include/lwip/opt.h for all options
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__LWIPOPTS_H__
#define __LWIP__LWIPOPTS_H__

/* Genode includes */
#include <base/fixed_stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Use lwIP without OS-awareness
 */
#define NO_SYS 1
#define SYS_LIGHTWEIGHT_PROT 0


/*
   ------------------------------------
   ---------- Memory options ----------
   ------------------------------------
*/

#define MEM_LIBC_MALLOC             1
/* Try disabling MEMP_MEM_MALLOC and check speed */
#define MEMP_MEM_MALLOC             1
/* MEM_ALIGNMENT > 4 e.g. for x86_64 are not supported, see Genode issue #817 */
#define MEM_ALIGNMENT               4

/* arch/cc.h overrides malloc symbols */

/* Pools are reported to be faster */
#define MEM_USE_POOLS                   0
#define MEMP_USE_CUSTOM_POOLS           0

/*
   ------------------------------------------------
   ---------- Internal Memory Pool Sizes ----------
   ------------------------------------------------
*/

#define MEMP_NUM_TCP_PCB           128

#define PBUF_POOL_SIZE                  96


/*
   ---------------------------------
   ---------- ARP options ----------
   ---------------------------------
*/

#define LWIP_ARP                        1


/*
   ----------------------------------
   ---------- DHCP options ----------
   ----------------------------------
*/

#define LWIP_DHCP                       1
#define LWIP_DHCP_CHECK_LINK_UP         1


/*
   ----------------------------------
   ---------- DNS options -----------
   ----------------------------------
*/

#define LWIP_DNS                        0


/*
   ---------------------------------
   ---------- TCP options ----------
   ---------------------------------
*/

#define TCP_MSS                         1460
#define TCP_WND                         (96 * TCP_MSS)
#define LWIP_WND_SCALE                  1
#define TCP_RCV_SCALE                   2
#define LWIP_TCP_TIMESTAMPS             1

/*
 * The window scale option (http://tools.ietf.org/html/rfc1323) patch of lwIP
 * definitely works solely for the receive window, not for the send window.
 * Setting the send window size to the maximum of an 16bit value, 65535,
 * or multiple of it (x * 65536 - 1) results in the same performance.
 * Everything else decrease performance.
 */
#define TCP_SND_BUF                 (65535)

#define TCP_SND_QUEUELEN            ((32 * (TCP_SND_BUF) + (TCP_MSS - 1))/(TCP_MSS))


/*
   ------------------------------------------------
   ---------- Network Interfaces options ----------
   ------------------------------------------------
*/

#define LWIP_NETIF_API                  0
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_LOOPBACK             1


/*
   ------------------------------------
   ---------- Thread options ----------
   ------------------------------------
*/

#define TCPIP_MBOX_SIZE                 128
#define DEFAULT_ACCEPTMBOX_SIZE         128

/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/
/* no Netconn API */
#define LWIP_NETCONN                    0


/*
   ------------------------------------
   ---------- Socket options ----------
   ------------------------------------
*/

/* no libc compatibility support, that is handled by vfs_lwip */
#define LWIP_SOCKET                     0


/*
   ----------------------------------------
   ---------- Statistics options ----------
   ----------------------------------------
*/

#define LWIP_STATS                      0

/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/

/* checksum calculation for outgoing packets can be disabled if the hardware supports it */
#define LWIP_CHECKSUM_ON_COPY           1


/*
   ---------------------------------------
   ---------- IPv6 options ---------------
   ---------------------------------------
*/

#define LWIP_IPV6                       1
#define IPV6_FRAG_COPYHEADER            1

#ifdef __cplusplus
}
#endif

#endif /* __LWIP__LWIPOPTS_H__ */
