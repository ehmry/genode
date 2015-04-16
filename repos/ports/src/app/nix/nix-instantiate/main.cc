/*
 * \brief  Simple native implemention of nix-instantiate
 * \author Emery Hemingway
 * \date   2015-01-14
 */

/* Genode includes */
#include <base/printf.h>
#include <os/config.h>

/* Nix native includes */
#include <nix/main.h>

/* Nix includes */
#include "attr-path.hh"
#include "common-opts.hh"
#include "eval.hh"
#include "get-drvs.hh"
#include "globals.hh"
#include "store-api.hh"
#include "util.hh"

#include <iostream>
#include <map>

using namespace nix;

void processExpr(EvalState & state, const Strings & attrPaths, Bindings & autoArgs, Expr * e)
{
	Value vRoot;
	state.eval(e, vRoot);

	foreach (Strings::const_iterator, i, attrPaths) {
		Value & v(*findAlongAttrPath(state, *i, autoArgs, vRoot));
		state.forceValue(v);

		PathSet context;
		DrvInfos drvs;
		getDerivations(state, v, "", autoArgs, drvs, false);
		foreach (DrvInfos::iterator, i, drvs) {
			Path drvPath = i->queryDrvPath();

			/* What output do we want? */
			string outputName = i->queryOutputName();
			if (outputName == "")
				throw Error(format("derivation ‘%1%’ lacks an ‘outputName’ attribute ") % drvPath);

			std::cout << format("%1%%2%\n") % drvPath % (outputName != "out" ? "!" + outputName : "");
		}
	}
}


int main(int argc, char **argv)
{
	/* Initialise common configuration. */
	nix::main::init();

	EvalState state(nix::main::search_path_from_config());

	std::map<string, string> autoArgs_;

	Bindings & autoArgs(*evalAutoArgs(state, autoArgs_));

	Strings attrPaths = nix::main::strings_from_config("attr");
	if (attrPaths.empty()) attrPaths.push_back("");

	Strings files = nix::main::strings_from_config("file");
	if (files.empty())  files.push_back("default.nix");

	store = openStore();

	foreach (Strings::iterator, i, files) {
		Expr * e = state.parseExprFromFile(resolveExprPath(lookupFileArg(state, *i)));
		processExpr(state, attrPaths, autoArgs, e);
	}

	state.printStats();
}
