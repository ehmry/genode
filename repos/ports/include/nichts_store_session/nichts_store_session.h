/*
 * \brief Nichts_store session interface
 * \author Emery Hemingway
 * \date   2015-03-07
 *
 */

#ifndef _INCLUDE__NICHTS_STORE_SESSION__NICHTS_STORE_SESSION_H_
#define _INCLUDE__NICHTS_STORE_SESSION__NICHTS_STORE_SESSION_H_

#include <session/session.h>
#include <base/rpc.h>
#include <base/rpc_args.h>


namespace Nichts_store {

	using namespace Genode;

	enum { MAX_PATH_LEN = 512 };

	enum Mode { NORMAL, REPAIR, CHECK };

	typedef Rpc_in_buffer<MAX_PATH_LEN> Path;

	/*
	 * Exception Types
	 */
	class Exception     : public Genode::Exception { };
	class Build_timeout : public Exception;
	class Build_failure : public Exception;

	struct Session : public Genode::Session
	{
		static const char *service_name() { return "Nichts_store"; }

		virtual ~Session() { }

		/**
		 * Realise the ouputs of a derivation file in the Nichts store
		 * according to the given mode.
		 *
		 * \throw Build_timeout  build process has timed out
		 * \throw Build_failure  build process has failed, this
		 *                       indicates a permanent failure
		 */
		virtual void realise(Path const  &drvPath, Mode mode) = 0;

		/*********************
		 ** RPC declaration **
		 *********************/

		/* TODO: make this function throw exceptions. */
		GENODE_RPC_THROW(Rpc_realise, void, realise,
		                 GENODE_TYPE_LIST(Build_timeout,
		                                  Build_failure)
		                 Path const&, Mode);
		GENODE_RPC_INTERFACE(Rpc_realise);
	};

}

#endif