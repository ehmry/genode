/*
 * \brief  Nichts_store session interface
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

	typedef String<MAX_PATH_LEN> Path;

	struct Exception  : Genode::Exception { };
	struct Invalid_derivation : Exception { };
	struct Build_timeout      : Exception { };
	struct Build_failure      : Exception { };

	struct Session : public Genode::Session
	{
		static const char *service_name() { return "Nichts_store"; }

		virtual ~Session() { }

		/**
		 * Hash a node opened with the store File_system
		 * service and return the final store path. This
		 * path will not be instantiated until the handle
		 * is closed on the File_system session.
		 */
		virtual Path hash(File_system::Node_handle) = 0;

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

		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_hash, Path, hash, File_system::Node_handle);
		GENODE_RPC_THROW(Rpc_realise, void, realise,
		                 GENODE_TYPE_LIST(Invalid_derivation,
		                                  Build_timeout, Build_failure),
		                 Path const&, Mode);
		GENODE_RPC_INTERFACE(Rpc_hash, Rpc_realise);
	};

}

#endif