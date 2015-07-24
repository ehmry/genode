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

#ifndef __FILE_SYSTEM__ROOT_H_
#define __FILE_SYSTEM__ROOT_H_

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
	 * The policy defaults to false, the arguments default to true.
	 */
	bool session_writeable(Session_policy policy, char const *args)
	{
		return
			policy.attribute_value("writeable", false) ?
			Arg_string::find_arg(args, "writeable").bool_value(true) :
			false;
	}
}

#endif
