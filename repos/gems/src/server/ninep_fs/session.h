 /*
  * \brief  9P client-side session
  * \author Emery Hemingway
  * \date   2015-04-12
  */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NINEP_CLIENT__CONN_H_
#define _NINEP_CLIENT__CONN_H_

/* Genode includes */
#include <base/env.h>
#include <base/exception.h>
#include <base/lock.h>
#include <util/string.h>

#include <timer_session/connection.h>

/* socket API */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

/* Local includes */
#include "9p.h"

namespace Plan9 {
	class Socket;
	class Attachment;
};

class Plan9::Socket
{
	public:
		struct Exception : Genode::Exception { };
		struct Temporary_failure : Exception { };
		struct Permanent_failure : Exception { };

		struct Slot
		{
			Fcall *rx;
			bool   done;
			char   error[ERRMAX];

			Slot(Fcall *response)
			: done(false)
			{
				rx = response;
				error[0] = 0;
			}

			void reset()
			{
				rx = 0;
				done = false;
				error[0] = 0;
			}
		};

	private:
		enum { MAX_SLOTS = 128, SLOT_MASK = 0x7F };

		Slot         *_slots[MAX_SLOTS];
		Lock          _slots_lock;
		Lock          _buf_lock;
		int           _fd;
		Plan9::size_t _msize;
		uint8_t      *_buf;

		void close()
		{
			Lock::Guard slots_guard(_slots_lock);
			Lock::Guard buf_guard(_buf_lock);
			::close(_fd); _fd = -1;

			/* Clear all pending transations. */
			for (int i = 0; i < MAX_SLOTS; ++i)
				if (_slots[i]) {
					Slot *slot = _slots[i];
					strncpy(slot->error, "socket closed", sizeof(slot->error));
					slot->done = true;
					_slots[i] = 0;
				}
		}

		Plan9::size_t read(uint8_t *buf, Plan9::size_t len)
		{
			if (_fd == -1) throw Exception();
			return ::read(_fd, (char*)buf, len);
		}

		Plan9::size_t write(uint8_t *buf, Plan9::size_t len)
		{
			if (_fd == -1) throw Exception();
			return ::write(_fd, (char*)buf, len);
		}

		void version(Plan9::size_t msize)
		{
			uint8_t buf[32];
			uint32_t msg_size;
			uint16_t msg_tag;
			uint16_t param_len = sizeof(Plan9::VERSION9P)-1;
			int bi = 0;

			put4(buf, bi, (4+1+2+4+2+param_len));
			put1(buf, bi, Plan9::Tversion);
			put2(buf, bi, Plan9::NOTAG);
			put4(buf, bi, msize);

			put2(buf, bi, param_len);
			memcpy(&buf[bi], Plan9::VERSION9P, param_len);
			bi += param_len;

			write(buf, bi);
			read(buf, sizeof(buf));

			bi = 0;
			get4(buf, bi, msg_size);
			if (msg_size < 7+4+2 || buf[4] != Rversion) {
				PERR("failed to negotiate protocol with server");
				throw Permanent_failure();
			}
			bi = 5;
			get2(buf, bi, msg_tag);
			get4(buf, bi, _msize);
			get2(buf, bi, param_len);

			buf[bi+param_len] = '\0';
			PLOG("Protocol version: %s, Message size: %d",
			     &buf[bi], _msize);
		}

	public:

		Socket() : _fd(-1)
		{
			for (int i = 0; i < MAX_SLOTS; ++i)
				_slots[i] = 0;
		}

		~Socket()
		{
			close();
			if (_buf) destroy(env()->heap(), _buf);
		}

		/**
		 * Message size negotiated with server.
		 */
		Plan9::size_t message_size() { return _msize; }

		bool connected() const { return _fd != -1; }

		/**
		 * Connect to the specified address and port
		 * and process incoming messages.
		 *
		 * \throw Temporary_failure
		 * \throw Permanent_failure
		 */
		void connect(char const *addr, int port, Plan9::size_t message_size)
		{
			if (_fd != -1) close();
			if (_buf) destroy(env()->heap(), _buf);

			_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (_fd == -1) {
				PERR("socket creation failed");
				throw Permanent_failure();
			}

			/* TODO: IPv6 */
			sockaddr_in sockaddr;
			sockaddr.sin_family = AF_INET;
			sockaddr.sin_port   = htons(port);
			if (inet_pton(AF_INET, addr, &sockaddr.sin_addr.s_addr) == 0) {
				PERR("bad IPv4 address %s for server", addr);
				close();
				throw Permanent_failure();
			}

			if (::connect(_fd, (struct sockaddr*) &sockaddr, sizeof(sockaddr))) {
				PERR("failed to connect to %s:%d", addr, port);
				close();
				throw Temporary_failure();
			}

			try {
				version(message_size);
			} catch (Temporary_failure) {
				/* If version fails, the server is incompatible. */
				throw Permanent_failure();
			}

			_buf = new (env()->heap()) uint8_t[_msize];
		}

		/**
		 * Register a transaction in _slots.
		 */
		tag_t queue(Slot *slot)
		{
			Genode::Lock::Guard slots_guard(_slots_lock);
			static tag_t i = 0;

			/*
			 * Use both bytes of the tag field for debugging,
			 * but rollover the lower bits for the slot index.
			 */
			while (1) {
				if (!_slots[i&SLOT_MASK]) {
					 _slots[i&SLOT_MASK] = slot;
					return i++;
				}
				i++;
			}

			PERR("too many concurrent 9P transactions");
			throw File_system::Out_of_node_handles();
			return NOTAG;
		}

		void send(Fcall *tx)
		{
			int len = 7;
			int str_len;
			Lock::Guard guard(_buf_lock);

			switch (tx->type) {
				case Tattach:
					put4(_buf, len, tx->fid);
					put4(_buf, len, tx->afid);
					str_len = strlen(tx->uname);
					put2(_buf, len, uint16_t(str_len));
					memcpy(&_buf[len], tx->uname, str_len);
					len += str_len;
					str_len = strlen(tx->aname);
					put2(_buf, len, uint16_t(str_len));
					len += str_len;
					memcpy(&_buf[len], tx->aname, str_len);
					len += str_len;
					break;

				case Twalk:
					put4(_buf, len, tx->fid);
					put4(_buf, len, tx->newfid);
					put2(_buf, len, tx->nwname);
					for (int i = 0; i < tx->nwname; ++i) {
						str_len = strlen(tx->wname[i]);
						put2(_buf, len, uint16_t(str_len));
						memcpy(&_buf[len], tx->wname[i], str_len);
						len += str_len;
					}
					break;

				case Topen:
					put4(_buf, len, tx->fid);
					put1(_buf, len, tx->mode);
					break;

				case Tcreate:
					put4(_buf, len, tx->fid);
					str_len = strlen(tx->name);
					put2(_buf, len, uint16_t(str_len));
					memcpy(&_buf[len], tx->name, str_len);
					len += str_len;
					put4(_buf, len, tx->perm);
					put1(_buf, len, tx->mode);
					break;

				case Tread:
					tx->count = min(tx->count, _msize-7+4);
					put4(_buf, len, tx->fid);
					put8(_buf, len, tx->offset);
					put4(_buf, len, tx->count);
					break;

				case Twrite:
					tx->count = min(tx->count, _msize-7+4+8+4);
					put4(_buf, len, tx->fid);
					put8(_buf, len, tx->offset);
					put4(_buf, len, tx->count);
					memcpy(&_buf[len], tx->data, tx->count);
					len += tx->count;
					break;

				case Tclunk:  put4(_buf, len, tx->fid); break;
				case Tremove: put4(_buf, len, tx->fid); break;
				case Tstat:   put4(_buf, len, tx->fid); break;

				case Twstat:
					put4(_buf, len, tx->fid);
					put2(_buf, len, tx->nstat);
					/* Overflow danger. */
					memcpy(&_buf[len], tx->stat, tx->nstat);
					len += tx->nstat;
					break;
			}

			int tmp = 0;
			/* Write the size, type, and tag. */
			put4(_buf, tmp, uint32_t(len));
			put1(_buf, tmp, tx->type);
			put2(_buf, tmp, tx->tag);

			write(_buf, len);
		}

		void receive()
		{
			using namespace Plan9;
			Lock::Guard guard(_buf_lock);

			Plan9::size_t size;
			Plan9::tag_t  tag;
			Plan9::type_t type;

			Plan9::size_t r = read(_buf, _msize);
			if (!r) {
				PERR("server closed connection");
				close();
				throw Exception();
			}

			uint16_t i = 0; /* Buffer index. */
			get4(_buf, i, size);
			while (r < size) /* Read the rest. */
				r += read(&_buf[r], size-r);

			get1(_buf, i, type);
			get2(_buf, i, tag);

			tag &= SLOT_MASK;
			if (tag > MAX_SLOTS) {
				/* That would be bad. */
				PERR("server replied with invalid tag %2d", tag);
				close();
				throw Temporary_failure();
			}

			_slots_lock.lock();
			Slot *slot = _slots[tag];
			_slots[tag] = 0;
			_slots_lock.unlock();

			if (slot == 0) return;

			if (type == Rerror) {
				uint16_t err_len;
				int i = 7;
				get2(_buf, i, err_len);
				_buf[9+err_len] = 0;
				strncpy(slot->error, (char*)&_buf[9], sizeof(slot->error));
			} else if (slot->rx) {
				Fcall *rx = slot->rx;
				rx->type = type;

				uint32_t data_len;
				int len = 7;
				switch (type) {
					case Rwalk: get2(_buf, len, rx->nwqid); break;

					case Rread:
						data_len = 0;
						get4(_buf, len, data_len);
						if (data_len > rx->count) {
							strncpy(slot->error, "rx->data is too small", sizeof(slot->error));
							break;
						}
						rx->count = data_len;
						memcpy(rx->data, &_buf[len], rx->count);
						break;

					case Rwrite: get4(_buf, len, rx->count); break;

					case Rstat:
						get2(_buf, len, rx->nstat);
						memcpy(rx->stat, &_buf[len], max(rx->nstat, 1024));
						break;
				}
			}
			slot->done = true;
		}
};


class Plan9::Attachment
{
	private:

		Socket &_sock;

	public:

		/**
		 * Constructor.
		 */
		Attachment(Socket &sock, Fid fid,
		           char const *uname, char const *aname)
		:
			_sock(sock)
		{

			Fcall tx;

			tx.fid   = fid;
			tx.afid  = Plan9::NOFID;
			tx.uname = uname;
			tx.aname = aname;

			transact(Plan9::Tattach, &tx, 0);
		}

		/**
		 * Message size negotiated with server.
		 */
		Plan9::size_t message_size() { return _sock.message_size(); }
		/**
		 * Send a request, receive a response.
		 */
		void transact(type_t type, Fcall *tx, Fcall *rx = 0)
		{
			Socket::Slot slot(rx);
			tx->type = type;
			tx->tag  = _sock.queue(&slot);

			_sock.send(tx);
			while (!slot.done) _sock.receive();

			if (slot.error[0]) {
				static char lookup[] = "No such file or directory";
				static char exists[] = "file or directory already exists";
				static char denied[] = "permission denied";

				if (strcmp(slot.error, lookup, sizeof(lookup)) == 0)
					throw File_system::Lookup_failed();

				if (strcmp(slot.error, exists, sizeof(exists)) == 0)
					throw File_system::Node_already_exists();

				if (strcmp(slot.error, denied, sizeof(denied)) == 0)
					throw File_system::Permission_denied();

				PERR("server replied '%s'", slot.error);
				throw Exception();

				if (rx && rx->type != ++type) {
					PERR("type or tag mismatch from server");
					throw Exception();
				}
			}
		}

		Fid duplicate(Fid oldfid)
		{
			Fcall tx;
			tx.fid      = oldfid;
			tx.newfid   = unique_fid();
			tx.nwname   = 0;
			tx.wname[0] = 0;

			transact(Plan9::Twalk, &tx, 0);
			return tx.newfid;
		}

		void create(Fid fid, char const *name, uint32_t perm, mode_t mode)
		{
			Fcall tx;

			/* Duplicate the fid. */
			tx.fid  = duplicate(fid);
			tx.name = name;
			tx.perm = perm;
			tx.mode = mode;
			transact(Plan9::Tcreate, &tx, 0);
			clunk(tx.fid);
		}

		void open(Fid fid, Plan9::mode_t mode)
		{
			Fcall tx;
			tx.fid = fid;
			tx.mode = mode;
			transact(Plan9::Topen, &tx, 0);
		}

		/**
		 * Determine the number of directory entries at the given fid.
		 */
		File_system::size_t num_entries(Fid fid)
		{
			/* A potentially excessive buffer.*/
			char buf[message_size() - Plan9::IOHDRSZ];
			Fcall tx, rx;
			File_system::size_t entries = 0;

			tx.fid    = duplicate(fid);;
			tx.offset = 0;
			tx.count  = rx.count = sizeof(buf);
			rx.data   = buf;

			open(tx.fid, Plan9::OREAD);

			while (1) {
				transact(Plan9::Tread, &tx, &rx);

				if (rx.count == 0) break;
				tx.offset += rx.count;

				for (int i = 0; i < int(rx.count);) {
					++entries;
					uint16_t stat_len = 0;
					get2(buf, i, stat_len);
					i += stat_len;
				}
			}
			clunk(tx.fid);
			return entries;
		}

		void clunk(Fid fid)
		{
			Fcall tx;
			tx.fid = fid;
			transact(Tclunk, &tx, 0);
		}
};

#endif