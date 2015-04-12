 /*
  * \brief  9P generic node
  * \author Emery Hemingway
  * \date   2015-04-12
  */

#ifndef _9P_CLIENT__NODE_H_
#define _9P_CLIENT__NODE_H_

/* Genode includes */
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>

/* Local includes */
#include "9p.h"

namespace File_system {

	class Listener : public List<Listener>::Element
	{
		private:

			Lock                      _lock;
			Signal_context_capability _sigh;
			bool                      _marked_as_updated;

		public:

			Listener() : _marked_as_updated(false) { }

			Listener(Signal_context_capability sigh)
			: _sigh(sigh), _marked_as_updated(false) { }

			void notify()
			{
				Lock::Guard guard(_lock);

				if (_marked_as_updated && _sigh.valid())
					Signal_transmitter(_sigh).submit();

				_marked_as_updated = false;
			}

			void mark_as_updated()
			{
				Lock::Guard guard(_lock);

				_marked_as_updated = true;
			}

			bool valid() const { return _sigh.valid(); }
	};


	class Node : public List<Node>::Element
	{
		private:

			Plan9::Fid     _fid;
			Lock           _lock;
			List<Listener> _listeners;
			bool           _modified;

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
				/* propagate event to listeners */
				mark_as_updated();
				notify_listeners();

				while (_listeners.first())
					_listeners.remove(_listeners.first());

				try { _attach.clunk(_fid); } catch (...) { }
			}

			Plan9::Fid fid() const { return _fid; }

			void lock()   { _lock.lock(); }
			void unlock() { _lock.unlock(); }

			virtual size_t read(char *dst, size_t, seek_off_t) {
				throw Invalid_handle(); };

			virtual size_t write(char *src, size_t, seek_off_t) {
				throw Invalid_handle(); };

			void add_listener(Listener *listener) {
				_listeners.insert(listener); }

			void remove_listener(Listener *listener) {
				_listeners.remove(listener); }

			void notify_listeners()
			{
				for (Listener *curr = _listeners.first(); curr; curr = curr->next())
					curr->notify();
			}

			void mark_as_updated()
			{
				for (Listener *curr = _listeners.first(); curr; curr = curr->next())
					curr->mark_as_updated();
			}

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

	/**
	 * Guard used for properly releasing node locks
	 */
	struct Node_lock_guard
	{
		Node &node;

		Node_lock_guard(Node &node) : node(node) { }

		~Node_lock_guard() { node.unlock(); }
	};

};

#endif /* _9P_CLIENT__NODE_H_ */