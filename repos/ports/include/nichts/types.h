#ifndef _NICHTS__TYPES_H_
#define _NICHTS__TYPES_H_

/* Stdcxx */
#include <string>
#include <list>
#include <map>
#include <set>

/* Genode includes. */
#include <util/string.h>


namespace Nichts {

	enum { MAX_PATH_LEN = 512; }

	typedef Genode::String<MAX_PATH_LEN> Path;

	/* Type used in Timer sessions. */
	typedef unsigned long time_t;

	using std::string;


	typedef std::list<string>        Strings;
	typedef std::set<string>         String_set;
	typedef std::map<string, string> String_map;
	typedef string                   Path;
	typedef std::list<Path>          Paths;
	typedef std::set<Path>           Path_set;

};

#endif /* _NICHTS__TYPES_H_ */
