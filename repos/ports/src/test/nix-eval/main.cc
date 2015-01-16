/*
 * \brief  Simple native evaluater
 * \author Emery Hemingway
 * \date   2015-01-13
 */

/* Genode includes */
#include <base/printf.h>
#include <os/config.h>

/* Nix includes */
#include "attr-path.hh"
#include "common-opts.hh"
#include "globals.hh"
#include "util.hh"
#include "eval.hh"

#include <map>
#include <iostream>


using namespace nix;

Strings strings_from_config(const char * name)
{
    Strings list;
    try {
	Genode::Xml_node node = Genode::config()->xml_node().sub_node(name);
	for (; ; node = node.next(name)) {
	    Genode::Xml_node::Attribute attr = node.attribute("name");

	    char cstr[attr.value_size()];
	    attr.value(cstr, sizeof(cstr));

	    list.push_back(string(cstr));
	}
    }
    catch (...) { }
    return list;
}

Strings search_path_from_config()
{
    Strings list;
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

Verbosity verbosity_from_config()
{
    return (nix::Verbosity) Genode::config()->xml_node().attribute_value<unsigned long>("verbosity", 0);
}

void processExpr(EvalState & state, const Strings & attrPaths, Bindings & autoArgs, Expr * e)
{
    Value vRoot;
    state.eval(e, vRoot);

    foreach (Strings::const_iterator, i, attrPaths) {
	Value & v(*findAlongAttrPath(state, *i, autoArgs, vRoot));
	state.forceValue(v);

	PathSet context;
	Value vRes;
	if (autoArgs.empty())
	    vRes = v;
	else
	    state.autoCallFunction(autoArgs, v, vRes);
	std::cout << vRes << std::endl;
    }
}

int main(int argc, char * * argv)
{
    settings.buildVerbosity = verbosity_from_config();
    settings.update();

    EvalState state(search_path_from_config());

    std::map<string, string> autoArgs_;

    Bindings & autoArgs(*evalAutoArgs(state, autoArgs_));

    Strings attrs = strings_from_config("attr");
    if (attrs.empty()) attrs.push_back("");

    Strings files = strings_from_config("file");
    if (files.empty())  files.push_back("default.nix");

    foreach (Strings::iterator, i, files) {
	Expr * e = state.parseExprFromFile(resolveExprPath(lookupFileArg(state, *i)));
	processExpr(state, attrs, autoArgs, e);
    }

    state.printStats();
}
