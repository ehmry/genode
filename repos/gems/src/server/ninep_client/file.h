 /*
  * \brief  9P file node
  * \author Emery Hemingway
  * \date   2015-04-12
  */

#ifndef _NINEP_CLIENT__FILE_H_
#define _NINEP_CLIENT__FILE_H_

/* Local includes */
#include "session.h"
#include "node.h"

namespace File_system {
	class File;
}

class File_system::File : public Node
{
	private:

		Plan9::Fid _io_fid;

	public:

		File(Plan9::Attachment &attachment, Plan9::Fid fid,
		     File_system::Mode mode)
		: Node(attachment, fid)
		{
			_io_fid = attachment.duplicate(fid);
			attachment.open(_io_fid, Plan9::convert_mode(mode));
		}

		virtual ~File() { try { attachment()->clunk(_io_fid); } catch (...) { } }

		size_t read(char *dst, size_t len, seek_off_t seek_offset)
		{
			File_system::size_t res = 0;
			Plan9::Fcall tx, rx;

			tx.fid = _io_fid;
			while (len) {
				tx.offset = seek_offset;
				tx.count = rx.count = len;
				rx.data = &dst[res];

				attachment()->transact(Plan9::Tread, &tx, &rx);

				if (rx.count == 0) break;
				res += rx.count;
				len -= rx.count;
			}
			return res;
		}

		size_t write(char *src, size_t len, seek_off_t seek_offset)
		{
			File_system::size_t res = 0;
			Plan9::Fcall tx, rx;

			tx.fid = _io_fid;
			while (len) {
				tx.offset = seek_offset;
				tx.count = rx.count = len;
				tx.data = &src[res];

				attachment()->transact(Plan9::Twrite, &tx, &rx);

				if (rx.count == 0) break;
				res += rx.count;
				len -= rx.count;
			}
			return res;
		}
};


#endif