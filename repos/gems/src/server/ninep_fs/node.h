 /*
  * \brief  9P generic node
  * \author Emery Hemingway
  * \date   2015-04-12
  */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _9P_CLIENT__NODE_H_
#define _9P_CLIENT__NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>

/* Local includes */
#include "9p.h"

namespace File_system {

	class Node : public Node_base, public List<Node>::Element
	{
		private:

			Plan9::Fid     _fid;
			Plan9::Attachment &_attach;

		protected:

			Plan9::Attachment *attachment() const { return &_attach; }

		public:
			Node(Plan9::Attachment &Attachment, Plan9::Fid fid)
			:
				_fid(fid),
				_attach(Attachment)
			{ }

			virtual ~Node() {
				try { _attach.clunk(_fid); } catch (...) { }
			}

			Plan9::Fid fid() const { return _fid; }

			virtual size_t read(char *dst, size_t, seek_off_t) {
				throw Invalid_handle(); };

			virtual size_t write(const char *src, size_t, seek_off_t) {
				throw Invalid_handle(); };

			void rename(File_system::Name const &name)
			{
				Plan9::Fcall tx;

				uint16_t str_len = name.size()-1;

				tx.fid = _fid;
				tx.nstat = Plan9::NULL_STAT_LEN+str_len;
				uint8_t buf[tx.nstat];
				tx.stat = (char*)buf;

				/* Zero off the extra space at the end. */
				memset(&buf[Plan9::NULL_STAT_LEN], 0x00, str_len);
				Plan9::zero_stat(buf);

				int bi = 2+4+13+4+4+4+8;
				put2(buf, bi, str_len);
				memcpy(&buf[bi], name.string(), str_len);

				_attach.transact(Plan9::Twstat, &tx);
			}
	};

};

#endif /* _9P_CLIENT__NODE_H_ */