/*
 * \brief Nichts_store session interface
 * \author Emery Hemingway
 * \date   2015-03-07
 */

#ifndef _INCLUDE__NICHTS_STORE_SESSION__NICHTS_STORE_SESSION_H_
#define _INCLUDE__NICHTS_STORE_SESSION__NICHTS_STORE_SESSION_H_

#include <base/exception.h>
#include <session/session.h>
#include <base/rpc.h>
#include <base/rpc_args.h>
#include <os/packet_stream.h>
#include <packet_stream_tx/packet_stream_tx.h>

#include <file_system_session/file_system_session.h>

namespace Nichts_store {

	using namespace Genode;

	enum { MAX_PATH_LEN = 512 };

	enum Mode { NORMAL, REPAIR, CHECK };

	typedef Rpc_in_buffer<MAX_PATH_LEN> Path;

	struct Exception  : Genode::Exception { };
	struct Invalid_derivation : Exception { };
	struct Build_timeout      : Exception { };
	struct Build_failure      : Exception { };

	struct Session : public Genode::Session
	{
		typedef File_system::Session::Tx Tx;

		static const char *service_name() { return "Nichts_store"; }

		virtual ~Session() { }

		/**
		 * Realise the ouputs of a derivation file in the Nichts store
		 * according to the given mode.
		 *
		 * \throw Invalid_derivation  derivation file was incompatible
		 *                            or failed to parse
		 * \throw Build_timeout       build process has timed out
		 * \throw Build_failure       build process has failed, this
		 *                            indicates a permanent failure
		 */
		virtual void realise(Path const  &drvPath, Mode mode) = 0;

		/**
		 * Return a file handle that can be used to write a file to
		 * the store. 
		 */
		virtual File_system::File_handle add_file(File_system::Name const &name) = 0;

		/**
		 * Close a file handle and return the hashed store path of the
		 * result.
		 *
		 * (TODO: the return)
		 */
		virtual void close_file(File_system::File_handle) = 0;

		/**
		 * Request client-side packet-stream interface of tx channel
		 * for importing files.
		 */
		virtual Tx::Source *tx() { return 0; }

		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_tx_cap, Capability<Tx>, _tx_cap);
		GENODE_RPC(Rpc_add_file, File_system::File_handle, add_file, File_system::Name const &);
		GENODE_RPC(Rpc_close_file, void, close_file, File_system::File_handle);
		GENODE_RPC_THROW(Rpc_realise, void, realise,
		                 GENODE_TYPE_LIST(Invalid_derivation,
		                                  Build_timeout, Build_failure),
		                 Path const&, Mode);
		GENODE_RPC_INTERFACE(Rpc_tx_cap, Rpc_add_file, Rpc_close_file, Rpc_realise);
	};

}

#endif