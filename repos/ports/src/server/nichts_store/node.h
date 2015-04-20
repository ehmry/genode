/*
 * \brief  File system proxy node
 * \author Emery Hemingway
 * \date   2015-04-20
 */

#ifndef _NICHTS_STORE__NODE_H_
#define _NICHTS_STORE__NODE_H_

namespace File_system { class Node; }

class File_system::Node
{
	private:

		Node_handle _handle;

	public:

		Node()
		{}

		~Node()
		{}

		virtual hash() = 0;
}

#endif