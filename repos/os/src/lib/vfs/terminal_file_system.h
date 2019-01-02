/*
 * \brief  Terminal file system
 * \author Christian Prochaska
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_
#define _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_

#include <terminal_session/connection.h>
#include <vfs/single_file_system.h>
#include <base/signal.h>
#include <base/registry.h>


namespace Vfs { class Terminal_file_system; }


class Vfs::Terminal_file_system : public Single_file_system
{
	private:

		typedef Genode::String<64> Label;
		Label _label;

		Genode::Env         &_env;

		Terminal::Connection _terminal { _env, _label.string() };

		struct Terminal_vfs_handle : Single_vfs_handle
		{
			Terminal::Connection &terminal;
			bool notifying = false;

			Terminal_vfs_handle(Terminal::Connection &term,
			                    Directory_service    &ds,
			                    File_io_service      &fs,
			                    Genode::Allocator    &alloc,
			                    int                   flags)
			:
				Single_vfs_handle(ds, fs, alloc, flags),
				terminal(term)
			{ }

			bool read_ready() override {
				return terminal.avail(); }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				if (!terminal.avail()) {
					notifying = true;
					return READ_QUEUED;
				}

				out_count = terminal.read(dst, count);
				return READ_OK;
			}

			Write_result write(char const *src, file_size count,
			                   file_size &out_count) override
			{
				out_count = terminal.write(src, count);
				return WRITE_OK;
			}
		};

		typedef Genode::Registered<Terminal_vfs_handle> Registered_handle;
		typedef Genode::Registry<Registered_handle>     Handle_registry;

		struct Post_signal_hook : Genode::Entrypoint::Post_signal_hook
		{
			Genode::Entrypoint  &_ep;
			Vfs_handle          *_handle = nullptr;

			Post_signal_hook(Genode::Entrypoint &ep) : _ep(ep) { }

			void arm(Vfs_handle &handle)
			{
				_handle = &handle;
				_ep.schedule_post_signal_hook(this);
			}

			void function() override
			{
				/*
				 * XXX The current implementation executes the post signal hook
				 *     for the last armed context only. When changing this,
				 *     beware that the called notify() may change
				 *     this object in a signal handler.
				 */

				if (_handle)
					_handle->notify();
				_handle = nullptr;
			}

			private:

				/*
				 * Noncopyable
				 */
				Post_signal_hook(Post_signal_hook const &);
				Post_signal_hook &operator = (Post_signal_hook const &);
		};

		Post_signal_hook _post_signal_hook { _env.ep() };

		Handle_registry _handle_registry { };

		Genode::Io_signal_handler<Terminal_file_system> _read_avail_handler {
			_env.ep(), *this, &Terminal_file_system::_handle_read_avail };

		void _handle_read_avail()
		{
			_handle_registry.for_each([this] (Registered_handle &h) {
				if (h.notifying)
					_post_signal_hook.arm(h);
			});
		}

	public:

		Terminal_file_system(Vfs::Env &env, Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_label(config.attribute_value("label", Label())),
			_env(env.env())
		{
			/* register for read-avail notification */
			_terminal.read_avail_sigh(_read_avail_handler);
		}

		static const char *name()   { return "terminal"; }
		char const *type() override { return "terminal"; }

		Open_result open(char const  *path, unsigned flags,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle = new (alloc)
					Registered_handle(_handle_registry, _terminal, *this, *this, alloc, flags);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		bool notify_read_ready(Vfs_handle *vfs_handle)
		{
			Terminal_vfs_handle *handle =
				static_cast<Terminal_vfs_handle*>(vfs_handle);
			if (!handle)
				return false;

			handle->notifying = true;
			return true;
		}

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}

		bool check_unblock(Vfs_handle *, bool rd, bool wr, bool) override
		{
			return ((rd && _terminal.avail()) || wr);
		}
};

#endif /* _INCLUDE__VFS__TERMINAL_FILE_SYSTEM_H_ */
