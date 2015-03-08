#ifndef _NICHTS__TYPES_H_
#define _NICHTS__TYPES_H_

/* Stdcxx */
#include <string>

namespace Nichts {

	/* Type used in Timer sessions. */
	typedef unsigned long time_t;

	using std::string;

	typedef string            Path;
	typedef std::list<string> Strings;
	typedef std::list<Path>   Paths

	struct Derivation_output {
		Path        path;
		std::string hash_algo; /* Hash used for expected hash computation. */
		std::string hash; /* Expected hash, may be null. */
	};

	struct Derivation {
		//DerivationOutputs outputs; /* keyed on symbolic IDs */
		//DerivationInputs inputDrvs; /* inputs that are sub-derivations */
		// ^ these might just be paths, since we build things one at a time */


		PathSet input_sources;
		Path builder;
		std::string system;
		Strings args;
		
		// attributes
		// outputs list
	};

};

#endif /* _NICHTS__TYPES_H_ */
