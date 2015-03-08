#include <rom_session/connection.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>

#include "worker.h"


static void env_node(Genode::Xml_generator xml, const char *name, const char *value)
{
	return xml.node("env", [&] {
		xml.attribute("name", name);
		xml.attribute("value", value);
	});
}

static void env_node(Genode::Xml_generator xml, const string name, const string value)
{
	return xml.node("env", [&] {
		xml.attribute("name", name.c_str());
		xml.attribute("value", value.c_str());
	});
}


namespace Nichts_store {

	Genode::Dataspace_capability noux_ds_cap()
	{
		try {
			static Genode::Rom_connection rom("noux");
			static Genode::Dataspace_capability noux_ds = rom.dataspace();
			return noux_ds;
		} catch (...) { PERR("failed to obtain Noux binary dataspace"); }

		return Genode::Dataspace_capability();
	}

	Worker::start_builder_noux(Derivation &drv)
	{
		PDBG("should be building with noux now");
	}

};