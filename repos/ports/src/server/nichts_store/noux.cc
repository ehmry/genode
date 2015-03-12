#include <rom_session/connection.h>
#include <file_system_session/file_system_session.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>

#include <nichts/util.h>

#include "worker.h"


static void env_node(Genode::Xml_generator xml, const char *name, const char *value)
{
	return xml.node("env", [&] {
		xml.attribute("name", name);
		xml.attribute("value", value);
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

};

void Nichts_store::Worker::start_builder_noux(Derivation &drv)
{
	char const *tmp_dir = create_temp_dir(Nichts::store_path_to_name(drv.path())).string();

	/*********************
	 ** Noux config ROM **
	 *********************/
	enum { CONFIG_SIZE = 4096 }; /* This may not be big enough */
	Genode::Ram_dataspace_capability config_ds = Genode::env()->ram_session()->alloc(CONFIG_SIZE);
	char* config_ds_addr = Genode::env()->rm_session()->attach(config_ds);
	Genode::Xml_generator xml(config_ds_addr, CONFIG_SIZE, "config", [&]
	{

		xml.attribute("verbose", "yes");

		xml.node("fstab", [&] {
			xml.node("fs");
		});

		xml.node("start", [&] {

			/*************
			 ** Builder **
			 *************/

			xml.attribute("name", drv.builder());
			xml.attribute("trace_syscalls", "yes");
			xml.attribute("verbose", "yes");

			/*****************
			 ** Environment **
			 *****************/

			/* Most shells initialise PATH to some default (/bin:/usr/bin:...) when
			* PATH is not set.  We don't want this, so we fill it in with some dummy
			* value.
			*/
			env_node(xml, "PATH", "/path-not-set");

			/* Set HOME to a non-existing path to prevent certain programs from using
			 * /etc/passwd (or NIS, or whatever) to locate the home directory (for
			 * example, wget looks for ~/.wgetrc).  I.e., these tools use /etc/passwd
			 * if HOME is not set, but they will just assume that the settings file
			 * they are looking for does not exist if HOME is set but points to some
			 * non-existing path.
			 */
			env_node(xml, "HOME", "/homeless-shelter");

			/* Tell the builder where the Nix store is.  Usually they
			 * shouldn't care, but this is useful for purity checking (e.g.,
			 * the compiler or linker might only want to accept paths to files
			 * in the store or in the build directory).
			 */
			env_node(xml, "NIX_STORE", "/nix/store");

			/* The maximum number of cores to utilize for parallel building. */
			env_node(xml, "NIX_BUILD_CORES", "1");

			/* For convenience, set an environment pointing to the top build
			 * directory.
			 */
			env_node(xml, "NIX_BUILD_TOP", tmp_dir);

			/* Also set TMPDIR and variants to point to this directory. */
			env_node(xml, "TMPDIR", tmp_dir);
			env_node(xml, "TEMPDIR", tmp_dir);
			env_node(xml, "TMP", tmp_dir);
			env_node(xml, "TEMP", tmp_dir);

			/* Set the working directory to the build directory. */
			env_node(xml, "NOUX_CWD", tmp_dir);

			/* Explicitly set PWD to prevent problems with chroot builds.  In
			 * particular, dietlibc cannot figure out the cwd because the
			 * inode of the current directory doesn't appear in .. (because
			 * getdents returns the inode of the mount point).
			 */
			env_node(xml, "PWD", tmp_dir);

			Derivation::Env_pair *drv_env = drv.env();
			while (drv_env) {
				char k[drv_env->key.len()];
				drv_env->key.string(k, sizeof(k));
				char v[drv_env->value.len()];
				drv_env->value.string(v, sizeof(v));

				env_node(xml, k, v);
				drv_env = drv_env->next();		
			}

		});
	});

	PLOG("%s", config_ds_addr);

	Genode::env()->rm_session()->detach(config_ds_addr);

}