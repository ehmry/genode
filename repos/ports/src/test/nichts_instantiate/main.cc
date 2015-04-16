/* Genode includes. */
#include <base/printf.h>
//#include <nichts_store_session/connection.h>
#include <os/config.h>

/* Nix upstream includes. */
#include <libutil/util.hh>
#include <libstore/globals.hh>
#include <libexpr/eval.hh>
#include <libexpr/eval-inline.hh>
#include <libexpr/get-drvs.hh>
#include <libexpr/attr-path.hh>

#include <libstore/store-api.hh>
#include <libexpr/common-opts.hh>
#include <libstore/misc.hh>


/* Stdcxx includes. */
#include <string>

namespace nix { };
using namespace nix;

static Verbosity verbosity_from_config()
{
	return (Verbosity) Genode::config()->xml_node().attribute_value<unsigned long>("verbosity", 0);
}

		static Strings strings_from_config(const char * name)
		{
			Strings list;
			try {
			Genode::Xml_node node = Genode::config()->xml_node().sub_node(name);
			for (; ; node = node.next(name)) {
				Genode::Xml_node::Attribute attr = node.attribute("name");

				char cstr[attr.value_size()+1];
				attr.value(cstr, sizeof(cstr));

				list.push_back(string(cstr));
				}
			}
			catch (...) { }
			return list;
		}

Strings search_path_from_config()
{
			std::list<string> list;
			try {
				Genode::Xml_node node = Genode::config()->xml_node().sub_node("search-path");
				for (; ; node = node.next("search-path")) {

					Genode::Xml_node::Attribute name_attr = node.attribute("name");
					Genode::Xml_node::Attribute path_attr = node.attribute("path");

					char str[name_attr.value_size() + path_attr.value_size() + 2];
					auto i = name_attr.value_size();
					name_attr.value(str, name_attr.value_size()+1);
					str[i++] = '=';
					path_attr.value(str+i, path_attr.value_size()+1);

					list.push_back(string(str));
				}
			}
			catch (...) { }
			return list;
}


extern "C" void wait_for_continue(void);

int main(int, char **)
{
	/* Initialise common configuration. */
	settings.processEnvironment();
	settings.loadConfFile();

	try {
		settings.buildVerbosity = verbosity_from_config();
		verbosity = settings.buildVerbosity;
	}
	catch (...) { }

	PLOG("opening store");
	store = openStore();
	PLOG("store opened");

	EvalState state(search_path_from_config());
	PLOG("state loaded");

	std::map<string, string> autoArgs_;

	Bindings & autoArgs(*evalAutoArgs(state, autoArgs_));

	PLOG("getting attrs");
	Strings attrPaths = strings_from_config("attr");
	if (attrPaths.empty()) attrPaths.push_back("");

	PLOG("opening store");
	store = openStore();
	PLOG("store opened");

	//Nichts_store::Connection srv;
	char text[4096];

	PLOG("entering loop");
	Genode::config()->xml_node().for_each_sub_node("expression", [&] (Genode::Xml_node node) {
		node.value(text, sizeof(text));

		PLOG("instantiating %s", text);
		Expr * e = state.parseExprFromString(std::string(text), absPath("/"));

		Value vRoot;
		state.eval(e, vRoot);

		foreach (Strings::const_iterator, i, attrPaths) {
			Value & v(*findAlongAttrPath(state, *i, autoArgs, vRoot));
			state.forceValue(v);

			PathSet context;
			DrvInfos drvs;
			getDerivations(state, v, "", autoArgs, drvs, false);
			foreach (DrvInfos::iterator, i, drvs) {
				Path drvPath = i-> queryDrvPath();

				// What output do we want?
				string outputName = i->queryOutputName();
				if (outputName == "")
					throw Error(format("derivation ‘%1%’ lacks an ‘outputName’ attribute ") % drvPath);
			}
		}
	});

	state.printStats();
}