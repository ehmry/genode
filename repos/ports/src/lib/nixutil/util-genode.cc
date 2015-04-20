/* Genode includes */
#include <os/config.h>
#include <base/printf.h>

#include <libutil/util.hh>

#include <cerrno>


namespace nix {

bool showStats() {
    try {
	return Genode::config()->xml_node().attribute("show-stats").has_value("true");
    }
    catch (Genode::Xml_node::Nonexistent_attribute) { return false; }
}


float cpuTime()
{
    PDBG("not implemented");
    return 0.0;
}


}
