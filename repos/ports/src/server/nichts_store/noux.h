/*
 * \brief  Noux builder details
 * \author Emery Hemingway
 * \date   2015-03-13
 */

#ifndef _NICHTS_STORE__NOUX_H_
#define _NICHTS_STORE__NOUX_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <util/xml_node.h>
#include <rom_session/connection.h>
#include <file_system_session/file_system_session.h>


/* Local includes */
#include "worker.h"


namespace Nichts_store {

	static void env_node(Genode::Xml_generator xml, const char *name, const char *value)
	{
		return xml.node("env", [&] {
			xml.attribute("name", name);
			xml.attribute("value", value);
		});
	}

	/**
	 * Write a config ROM for Noux to a dataspace.
	 */
	void noux_config(Dataspace_capability config_ds, int len, Derivation &drv)
	{
		char* config_ds_addr = Genode::env()->rm_session()->attach(config_ds);
		Genode::Xml_generator xml(config_ds_addr, len, "config", [&]
		{
			xml.attribute("verbose", "yes");

			xml.node("fstab", [&] {
				xml.node("fs");
			});

			xml.node("start", [&] {

				/*************
				 ** Builder **
				 *************/

				char builder[File_system::MAX_PATH_LEN];
				drv.builder(builder, sizeof(builder));
				xml.attribute("name", builder);
				xml.attribute("trace_syscalls", "yes");
				xml.attribute("verbose", "yes");

				/***************
				 ** Arguments **
				 ***************/
				Parser::String *arg = drv.arg();
				while (arg) {
					char buf[arg->len()+1];
					arg->string(buf, sizeof(buf));

					xml.node("arg", [&] {
						xml.attribute("value", buf);
					});
					arg = arg->next();
				}

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
				env_node(xml, "NIX_BUILD_TOP", "/tmp");

				/* Also set TMPDIR and variants to point to this directory. */
				env_node(xml, "TMPDIR", "/tmp");
				env_node(xml, "TEMPDIR", "/tmp");
				env_node(xml, "TMP",  "/tmp");
				env_node(xml, "TEMP", "/tmp");

				/* Set the working directory to the build directory. */
				env_node(xml, "NOUX_CWD", "/tmp");

				/* Explicitly set PWD to prevent problems with chroot builds.  In
				 * particular, dietlibc cannot figure out the cwd because the
				 * inode of the current directory doesn't appear in .. (because
				 * getdents returns the inode of the mount point).
				 */
				env_node(xml, "PWD", "/tmp");

				Derivation::Env_pair *drv_env = drv.env();
				while (drv_env) {
					char k[drv_env->key.len()+1];
					drv_env->key.string(k, sizeof(k));
					char v[drv_env->value.len()+1];
					drv_env->value.string(v, sizeof(v));

					env_node(xml, k, v);
					drv_env = drv_env->next();
				}
			});
		});

		PLOG("%s", config_ds_addr);
		Genode::env()->rm_session()->detach(config_ds_addr);
	};

}

#endif