/*
 * \brief  Service that writes log messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes. */
#include <log_session/log_session.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <root/component.h>
#include <os/server.h>
#include <os/session_policy.h>

#include <base/printf.h>

namespace Log_file {

	using namespace Genode;

	/* Maxium length of a string passed in a LOG call. */
	enum { MAX_MSG_LEN = 256 };

	class  Session_component;
	class  Log_root_component;
	struct Main;

}

class Log_file::Session_component : public Rpc_object<Log_session>
{
	private:

		File_system::Session    &_fs;
		File_system::File_handle _file_handle;
		Lock                     _offset_lock;
		File_system::seek_off_t  _offset;

	public:

		/**
		 * Constructor
		 */
		Session_component(File_system::Session    &fs,
		                  File_system::File_handle fh,
		                  File_system::seek_off_t  offset)
		: _fs(fs), _file_handle(fh), _offset(offset) { }

		~Session_component()
		{
			_fs.close(_file_handle);
		}

		/*****************
		 ** Log session **
		 *****************/

		/**
		 * Copy the log message to a packet and send it to the file system.
		 *
		 * This method returns before the write is acknowledged
		 * for the benefit of buffering.
		 */
		size_t write(Log_session::String const &string)
		{
			if (!(string.is_valid_string())) {
				PERR("corrupted string");
				return 0;
			}

			File_system::Session::Tx::Source &source = *_fs.tx();

			char const *msg  = string.string();
			size_t msg_len   = Genode::strlen(msg);
			size_t write_len = msg_len;

			/*
			 * If the message did not fill the incoming buffer
			 * make space to add a newline.
			 */
			if ((msg_len < MAX_MSG_LEN) && (msg[msg_len-1] != '\n'))
				++write_len;

			/* Protect the offset. */
			_offset_lock.lock();
			File_system::Packet_descriptor
				packet(source.alloc_packet(MAX_MSG_LEN),
				       0, /* The result struct. */
			    	   _file_handle,
			    	   File_system::Packet_descriptor::WRITE,
			    	   write_len,
			    	   _offset);
			_offset += write_len;
			_offset_lock.unlock();

			char *buf = source.packet_content(packet);
			memcpy(buf, msg, msg_len);

			if (msg_len != write_len)
				buf[msg_len] = '\n';

			collect_acknowledgements(source);
			source.submit_packet(packet);
			return msg_len;
		}

};

class Log_file::Log_root_component : public Genode::Root_component<Log_file::Session_component>
{
	private:

		Allocator_avl           _write_alloc;
		File_system::Connection _fs;

		File_system::File_handle open_file(File_system::Dir_handle &dir_handle,
		                                   char const *name)
		{
			try {
				return _fs.file(dir_handle, name, File_system::WRITE_ONLY, false);
			} catch (File_system::Lookup_failed) {
				return _fs.file(dir_handle, name, File_system::WRITE_ONLY, true);
			}
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace File_system;

			/*
			 * TODO:
			 * A client registry for opened file handles.
			 * Without this two clients could write over each other
			 * when the offsets get out of sync.
			 */

			Genode::Path<MAX_PATH_LEN-MAX_NAME_LEN> path;
			char name[MAX_NAME_LEN];
			name[0] = 0;

			Session_label session_label(args);
			char const *label = session_label.string();

			Dir_handle   dir_handle;
			File_handle file_handle;
			bool truncate = false;

			size_t len = strlen(label);
			size_t start = 0;
			for (size_t i = 1; i < len;) {
				if (strcmp(" -> ", label+i, 4) == 0) {
					strncpy(name, label+start, min((i-start)+1, sizeof(name)));
					path.append(name);
					i += 4;
					start = i;
				} else ++i;
			}
			strncpy(name, label+start, min((len-start)+1, sizeof(name)));
			strncpy(name+(len-start), ".log",
			        min(size_t(sizeof(".log")), size_t(sizeof(name))-(len-start)));

			try {
				Session_policy policy(session_label);

				try {
					char  dirname[MAX_PATH_LEN-MAX_NAME_LEN];
					policy.attribute("dir").value(dirname, sizeof(dirname));
					if (dirname[0] != '/') {
						PERR("Session log directory `%s' is not absolute", dirname);
						throw Root::Unavailable();
					}
					dir_handle = ensure_dir(_fs, dirname);
				} catch (Xml_node::Nonexistent_attribute) { }

				try {
					policy.attribute("file").value(name, sizeof(name));
				} catch (Xml_node::Nonexistent_attribute) { }

				try {
					truncate = policy.attribute("truncate").has_value("yes");
				} catch (Xml_node::Nonexistent_attribute) { }

			} catch (Session_policy::No_policy_defined) { }

			if (!dir_handle.valid())
				dir_handle = ensure_dir(_fs, path.base());
			Handle_guard dir_guard(_fs, dir_handle);

			if (!file_handle.valid())
				file_handle = open_file(dir_handle, name);

			seek_off_t offset = 0;
			if (truncate)
				_fs.truncate(file_handle, 0);
			else
				offset = _fs.status(file_handle).size;

			return new (md_alloc()) Session_component(_fs, file_handle, offset);
		}

	public:

		/**
		 * Constructor
		 *
		 * TODO:
		 * Use all of the available ram for the transmission buffer.
		 * This means the amount of log buffering is determined by
		 * the quota received from the parent.
		 */
		Log_root_component(Server::Entrypoint &ep, Allocator &alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &alloc),
			_write_alloc(&alloc),
			_fs(_write_alloc, MAX_MSG_LEN * 8)
		{ }

};

struct Log_file::Main
{
	Server::Entrypoint &ep;

	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Log_root_component root { ep, sliced_heap };

	Main(Server::Entrypoint &ep)
	: ep(ep)
	{
		Genode::env()->parent()->announce(ep.manage(root));
	}
};


/************
 ** Server **
 ************/

namespace Server {

	char const* name() { return "log_file_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep) { static Log_file::Main inst(ep); }

}