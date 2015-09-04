/*
 * \brief  File system root component utilities
 * \author Emery Hemingway
 * \date   2015-07-25
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/session_policy.h>
#include <root/component.h>


namespace File_system {

	/**
	 * Determine a root path by applying session
	 * policy followed by session arguments.
	 */
	void session_root_path(char           *path,
	                       size_t          path_len,
	                       Session_policy  policy,
	                       char     const *args)
	{
		if (path_len)
			path[0] = '\0';
		else
			return;

		try {
			policy.attribute("root").value(path, path_len);
		} catch (Xml_node::Nonexistent_attribute) {
			Session_label label(args);
			PERR("Missing \"root\" policy definition for %s",
			     label.string());
			throw Root::Unavailable();
		}

		if (path[0] != '/') {
			Session_label label(args);
			PERR("invalid policy root \"%s\" for %s", path, label.string());
			throw Root::Unavailable();
		}

		if (strcmp("/", path) == 0)
			Arg_string::find_arg(args, "root").string(
				path, path_len, "/");
		else {
			/* Shift down our working path string */
			size_t start = strlen(path);
			path += start;
			path_len -= start;

			Arg_string::find_arg(args, "root").string(
				path, path_len, "");

			if ((path[0] == '/') && (path[1] == '\0')) {
				/*
				 * no additional path, strip the
				 * trailing slash and return
				 */
				path[0] = '\0';
				return;
			}
		}

		if (path[0] != '/') {
			Session_label label(args);
			PERR("invalid root \"%s\" requested by %s", path, label.string());
			throw Root::Invalid_args();
		}
	}

	/**
	 * Determine if the session is writeable by checking
	 * the policy and session arguments.
	 *
	 * The policy defaults to false, the arguments to true.
	 */
	bool session_writeable(Session_policy policy, char const *args)
	{
		if (!policy.bool_attribute("writeable"))
			return false;

		/* policy allows, do the args? */

		return Arg_string::find_arg(args, "writeable").bool_value(true);
	}

	/**
	 * Lookup the "tx_buf_size" from session arguments
	 * and check that the ram quota is sufficient for
	 * the buffer and size of the session component.
	 */
	size_t session_tx_buf_size(size_t component_size, char const *args)
	{
		size_t ram_quota =
			Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
		size_t tx_buf_size =
			Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

		if (!tx_buf_size) {
			Session_label label(args);
			PERR("session with a null transmission buffer requested by %s",
			     label.string());
			throw Root::Invalid_args();
		}

		/*
		 * Check if donated ram quota suffices for session data,
		 * and communication buffer.
		 */
		size_t session_size = component_size + tx_buf_size;
		if (max((size_t)4096, session_size) > ram_quota) {
			Session_label label(args);
			PERR("insufficient 'ram_quota' from %s, got %zd, need %zd",
			     label.string(), ram_quota, session_size);
			throw Root::Quota_exceeded();
		}

		return tx_buf_size;
	}

}
