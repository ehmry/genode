/*
 * \brief  Config loading for nix utilities
 * \author Emery Hemingway
 * \date   2015-02-06
 */

/* Genode includes */
#include <base/printf.h>
#include <os/config.h>

#include <nix/main.h>

/* Upstream includes */
#include <types.hh>

namespace nix {

	namespace main {

		Strings strings_from_config(const char * name)
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

		Verbosity verbosity_from_config()
		{
		    return (nix::Verbosity) Genode::config()->xml_node().attribute_value<unsigned long>("verbosity", 0);
		}

	} /* namespace main */

} /* namespace nix */