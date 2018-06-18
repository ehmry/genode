/* Copyright (c) 2014 Alexander Lamaison <alexander.lamaison@gmail.com>
 * Copyright (c) 1999-2011 Douglas Gilbert. All rights reserved.
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 *   Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials
 *   provided with the distribution.
 *
 *   Neither the name of the copyright holder nor the names
 *   of any other contributors may be used to endorse or
 *   promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/* Headers */
#define HAVE_UNISTD_H
#define HAVE_INTTYPES_H
#define HAVE_STDLIB_H
#define HAVE_SYS_SELECT_H
#define HAVE_SYS_UIO_H
#define HAVE_SYS_SOCKET_H
#define HAVE_SYS_IOCTL_H
#define HAVE_SYS_TIME_H
#define HAVE_SYS_UN_H
#define HAVE_NTDEF_H
#define HAVE_NTSTATUS_H

/* Libraries */
#define HAVE_LIBCRYPT32

/* Types */
#define HAVE_LONGLONG

/* Functions */
#define HAVE_GETTIMEOFDAY
#define HAVE_INET_ADDR
#define HAVE_POLL
#define HAVE_SELECT
#define HAVE_SOCKET
#define HAVE_STRTOLL
#define HAVE_STRTOI64
#define HAVE_SNPRINTF

/* OpenSSL is available, so use it */
#define LIBSSH2_OPENSSL
#define HAVE_EVP_AES_128_CTR

/* Socket non-blocking support */
#define HAVE_O_NONBLOCK
#define HAVE_FIONBIO
#define HAVE_IOCTLSOCKET
#define HAVE_IOCTLSOCKET_CASE
#define HAVE_SO_NONBLOCK
#define HAVE_DISABLED_NONBLOCKING

#define HAVE_SNPRINTF

