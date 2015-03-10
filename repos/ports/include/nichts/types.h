#ifndef _NICHTS__TYPES_H_
#define _NICHTS__TYPES_H_

/* Stdcxx */
#include <string>
#include <map>

namespace Nichts {

	/* Type used in Timer sessions. */
	typedef unsigned long time_t;

	using std::string;


	typedef std::list<string>        Strings;
	typedef std::set<string>         String_set;
	typedef std::map<string, string> String_map;
	typedef string                   Path;
	typedef std::list<Path>          Paths;
	typedef std::set<Path>           Path_set;


	struct Derivation_output {
		Path        path;
		std::string hash_algo; /* Hash used for expected hash computation. */
		std::string hash; /* Expected hash, may be null. */
	};

	typedef std::map<string, Derivation_output> Derivation_outputs;
	typedef std::map<Path, String_set>          Derivation_inputs;

	/* For inputs that are sub-derivations, 
	 * we specify exactly which output IDs
	 * we are interested in.
	 */
	typedef std::map<Path, String_set> Derivation_inputs;

	struct Derivation {
		Derivation_outputs outputs; /* keyed on symbolic IDs */
		Derivation_inputs  input_drvs; /* inputs that are sub-derivations */
		Path_set           input_sources;
		string             platform;
		Path               builder;
		Strings            args;
		String_map         attrs;
	};


};

#endif /* _NICHTS__TYPES_H_ */
