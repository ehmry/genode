 /*
  * \brief  9P directory node
  * \author Emery Hemingway
  * \date   2015-04-12
  */

#ifndef _NINEP_CLIENT__DIRECTORY_H_
#define _NINEP_CLIENT__DIRECTORY_H_

/* Local includes */
#include "9p.h"
#include "session.h"
#include "node.h"
#include "file.h"

namespace File_system {
	class Directory;
}

class File_system::Directory : public File_system::Node
{
	private:

		/*
		 * A fid opened for I/O cannot be walked from.
		 */
		Plan9::Fid _io_fid;

		/* Directory read index. */
		seek_off_t    _dir_ind;
		/* Directory read offset. */
		Plan9::size_t _dir_off;

	public:

		Directory(Plan9::Attachment &attachment, Plan9::Fid fid)
		:
			Node(attachment, fid), _dir_ind(0), _dir_off(0)
		{
			_io_fid = attachment.duplicate(fid);
			attachment.open(_io_fid, Plan9::OREAD);
		}

		virtual ~Directory() { try { attachment()->clunk(_io_fid); } catch (...) { } }

		Plan9::Fid walk_name(char *name)
		{
			Plan9::Fcall tx;
			tx.fid = fid();
			tx.newfid = Plan9::unique_fid();
			tx.nwname = 1;
			tx.wname[0] = name;

			attachment()->transact(Plan9::Twalk, &tx);

			return tx.newfid;
		}

		size_t read(char *dst, size_t len, seek_off_t seek_offset)
		{
			char buf[1024];
			Plan9::Fcall tx, rx;
			size_t res = 0;

			if (len < sizeof(Directory_entry)) {
				PERR("read buffer too small for directory entry");
				return 0;
			}

			if (seek_offset % sizeof(Directory_entry)) {
				PERR("seek offset not aligned to sizeof(Directory_entry)");
				return 0;
			}

			seek_off_t index = seek_offset / sizeof(Directory_entry);

			if (index >= _dir_ind) {
				/* Rollback and start from the begining. */
				_dir_ind = 0;
				_dir_off = 0;
			}

			tx.fid   = _io_fid;
			tx.count = rx.count = sizeof(buf);
			rx.data  = buf;

			while (res  < len) {
				tx.offset = _dir_off;

				attachment()->transact(Plan9::Tread, &tx, &rx);

				_dir_off += rx.count;
				uint16_t bi = 0;
				while (bi < rx.count) {
					uint16_t stat_len = 0;
					get2(buf, bi, stat_len);

					if (bi+stat_len > rx.count) {
						PERR("recieved malformed directory entry");
						return res;
					}
					if (_dir_ind == index) {
						++index;
						int i = bi+2+4;
						Directory_entry *e = (Directory_entry *)(&dst[res]);
						e->type = buf[i] & 0x80
							? Directory_entry::TYPE_DIRECTORY
							: Directory_entry::TYPE_FILE;
						i += 13+4+4+4+8;
						uint16_t str_len;
						get2(buf, i, str_len);
						memcpy(e->name, &buf[i], str_len);
						e->name[str_len] = '\0';

						res += sizeof(Directory_entry);
					}
					bi += stat_len;
					++_dir_ind;
				}
			}

			return res;
		}

		/**
		 * Directories may not be written.
		 */
		size_t write(char *src, size_t len, seek_off_t seek_offset) {
			return 0; }
};

#endif /* _NINEP_CLIENT__DIRECTORY_H_ */