/*
 * \brief  Terminal file system
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_
#define _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_

#include <terminal_session/connection.h>
#include <vfs/single_file_system.h>

namespace Vfs { class Terminal_file_system; }


class Vfs::Terminal_file_system : public Single_file_system
{
	private:

		struct Handle final : Vfs_handle, Genode::List<Handle>::Element
		{
			using Vfs_handle::Vfs_handle;
		};

		typedef Genode::String<64> Label;
		Terminal::Connection _terminal;

		Genode::Signal_handler<Terminal_file_system> _notifier;

		Genode::List<Handle> _subscribers;

		unsigned _poll_state = 0;

		void _notify_all()
		{
			_poll_state |= Poll::READ_READY;

			for (Handle *h = _subscribers.first(); h; h = h->next())
				h->notify_callback();
		}

	public:

		static const char *name() { return "terminal"; }

		Terminal_file_system(Genode::Env &env,
		                     Genode::Allocator&,
		                     Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(),
			                   config, OPEN_MODE_RDWR),
			_terminal(env, config.attribute_value("label", Label()).string()),
			_notifier(env.ep(), *this, &Terminal_file_system::_notify_all)
		{
			/*
			 * Wait for connection-established signal
			 *
			 * TODO: return immediatly, but postpone callbacks until connected
			 */

			/* create signal receiver, just for the single signal */
			Genode::Signal_context    sig_ctx;
			Genode::Signal_receiver   sig_rec;
			Signal_context_capability sig_cap = sig_rec.manage(&sig_ctx);

			/* register signal handler */
			_terminal.connected_sigh(sig_cap);

			/* wait for signal */
			sig_rec.wait_for_signal();
			sig_rec.dissolve(&sig_ctx);

			_terminal.read_avail_sigh(_notifier);
		}


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const  *path,   unsigned     mode,
		                 Vfs_handle **handle, Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			if (mode & Vfs::Directory_service::OPEN_MODE_CREATE)
				return OPEN_ERR_EXISTS;

			*handle = new (alloc) Handle(*this, *this, alloc, 0);

			return OPEN_OK;
		}


		void close(Vfs_handle *vfs_handle) override
		{
			Handle *handle = static_cast<Handle*>(vfs_handle);
			_subscribers.remove(handle);
			destroy(handle->alloc(), handle);
		}


		bool subscribe(Vfs_handle *vfs_handle) override
		{
			Handle *handle = static_cast<Handle*>(vfs_handle);
			for (Handle *h = _subscribers.first(); h; h = h->next())
				if (h == handle) return true;
			_subscribers.insert(handle);
			return true;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		void write(Vfs_handle *handle, file_size len) override
		{
			auto func = [&] (char *buf, Genode::size_t buf_len)
			{
				handle->write_callback(buf, buf_len, Callback::PARTIAL);
			};

			_terminal.write(func, len);
			handle->write_status(Callback::COMPLETE);
		}

		void read(Vfs_handle *handle, file_size len) override
		{
			/* the amount of availabel data is unknown, so reset the state */
			_poll_state = 0;

			auto func = [&] (char const *buf, Genode::size_t buf_len)
			{
				handle->read_callback(buf, len, Callback::PARTIAL);
			};

			_terminal.read(func, len);
			handle->read_callback(nullptr, 0, Callback::COMPLETE);
		}

		Ftruncate_result ftruncate(Vfs_handle *vfs_handle, file_size) override
		{
			return FTRUNCATE_OK;
		}

		unsigned poll(Vfs_handle *handle) override
		{
			return
				(_poll_state ? _poll_state :
					(_terminal.avail() ? Poll::READ_READY : 0)) |
				Poll::WRITE_READY;
		}
};

#endif /* _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_ */
