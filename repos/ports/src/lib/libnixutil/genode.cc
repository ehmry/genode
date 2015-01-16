/* Genode includes */
#include <cap_session/connection.h>
#include <cpu_session/cpu_session.h>
#include <os/config.h>


namespace nix {

bool showStats() {
    try {
	return Genode::config()->xml_node().attribute("show-stats").has_value("true");
    }
    catch (Genode::Xml_node::Nonexistent_attribute) { return false; }
}

float cpuTime()
{
    Genode::size_t x = Genode::env()->cpu_session()->used(); /* << Genode::Cpu_session::QUOTA_LIMIT_LOG2; */
    if (x == 0)
	PDBG("not implemented");
    return x;
}

}
