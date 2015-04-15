#include <nix/main.h>

#include <base/printf.h>

/* Upstream includes */
#include <libstore/globals.hh>

namespace nix {

	extern Verbosity verbosity;

	namespace main {

		void init()
		{
			settings.processEnvironment();
			settings.loadConfFile();

			try {
				settings.buildVerbosity = verbosity_from_config();
				verbosity = settings.buildVerbosity;
			}
			catch (...) { }

			/* Do something to handle stack overflows? */
		}

	}

}