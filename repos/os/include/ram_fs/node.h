/*
 * \brief  File-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RAM_FS__NODE_H_
#define _INCLUDE__RAM_FS__NODE_H_

/* Genode includes */
#include <file_system/listener.h>
#include <file_system/node.h>
#include <util/list.h>

namespace File_system { class Node; }


class File_system::Node : public Node_base, public Genode::List<Node>::Element
{
	public:

		typedef Genode::String<128> Name;

	private:

		/**
		 * Generate unique inode number
		 */
		static unsigned long _unique_inode()
		{
			static unsigned long inode_count;
			return ++inode_count;
		}

		Name                _name;
		unsigned long const _inode = _unique_inode();
		int                 _ref_count = 0;

	public:

		unsigned long inode() const { return _inode; }
		char   const *name()  const { return _name.string(); }

		/**
		 * Assign name
		 */
		void name(char const *name) { _name = name; }

		virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
		virtual size_t write(char const *src, size_t len, seek_off_t) = 0;
};

#endif /* _INCLUDE__RAM_FS__NODE_H_ */
