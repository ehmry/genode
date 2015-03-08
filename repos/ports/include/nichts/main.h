/*
 * \brief  Common code for Nichts utilities
 * \author Emery Hemingway
 * \date   2015-02-06
 */

#ifndef _INCLUDE__NICHTS__MAIN__MAIN_H_
#define _INCLUDE__NICHTS__MAIN__MAIN_H_

#include <types.hh>

namespace Nichts {

	namespace Main {

		/**
		 * Return a list of strings from the config session.
		 * \param name  collect nodes that match name
		 *
		 * \return      list of values found under a 'name' attribute
		 */
		Strings strings_from_config(const char *name);

		/**
		 * Return a list of strings in the form "name=path",
		 * where 'name' and 'path' are attributes found in
		 * 'search-path' nodes.
		 */
		Strings search_path_from_config();

		/**
		 * Return a verbosity level from the config session.
		 */
		Verbosity verbosity_from_config();

		/**
		 * Initialise global variables from config session
		 */
		void init();
	}

}

#endif /* _INCLUDE__NICHTS__MAIN__MAIN_H_ */