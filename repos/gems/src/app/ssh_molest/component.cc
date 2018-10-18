/*
 * \brief  Utility for stressing SSH servers
 * \author Emery Hemingway
 * \date   2018-10-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/reconstructible.h>
#include <libc/component.h>
#include <base/sleep.h>
#include <util/xml_generator.h>

/* Libssh includes */
#include <libssh/libssh.h>
#include <libssh/callbacks.h>

/* Libc includes */
#include <stdio.h>
#include <stdlib.h>


namespace Ssh_client {
	using namespace Genode;
	struct Main;

	typedef Genode::String<64> String;

	String string_attr(Xml_node const &node,
	                   char const *key, char const *def = "") {
		return node.attribute_value(key, String(def)); }
}


struct Ssh_client::Main
{
	Libc::Env &_env;

	typedef Genode::String<128> String;
	String _hostname { };
	String _password { };

	/* must the host be known */
	bool _host_known = true;

	void _exit(int code)
	{
		ssh_finalize();
		_env.parent().exit(code);
		Genode::sleep_forever();
	}

	static void _log_host_usage()
	{
		char buf[1024];
		Xml_generator gen(buf, sizeof(buf), "host", [&gen] () {
			gen.attribute("name", "...");
			gen.attribute("port", 22);
			gen.attribute("user", "...");
			gen.attribute("pass", "...");
			gen.attribute("known", "yes");
		});

		log("host file format: ", Xml_node(buf, gen.used()));
	}

	void _configure(ssh_session &session)
	{
		using namespace Genode;

		_env.config([&] (Xml_node const &config) {
			int verbosity = config.attribute_value("verbose", false)
				? SSH_LOG_FUNCTIONS : SSH_LOG_NOLOG;
			ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
		});

		/* read all files from the root directory */
		ssh_options_set(session, SSH_OPTIONS_SSH_DIR, "/");
		ssh_options_set(session, SSH_OPTIONS_KNOWNHOSTS, "/known_hosts");

		/* read the XML formatted host file */
		char buf[4096];
		FILE *f = fopen("host", "r");
		if (f == NULL) {
			error("failed to open \"/host\" configuration file");
			_log_host_usage();
			_exit(~0);
		}
		size_t n = fread(buf,	1, sizeof(buf), f);
		fclose(f);

		Xml_node host_cfg(buf, n);
		try {
			host_cfg.attribute("name").value(&_hostname);
			ssh_options_set(session, SSH_OPTIONS_HOST,
				_hostname.string());
			ssh_options_set(session, SSH_OPTIONS_PORT_STR,
				string_attr(host_cfg, "port").string());
			ssh_options_set(session, SSH_OPTIONS_USER,
				string_attr(host_cfg, "user").string());
			_password = host_cfg.attribute_value("pass", String());
			_host_known = host_cfg.attribute_value("known", true);
		}
		catch (...) {
			error("failed to parse host configuration");
			error(host_cfg);
			_log_host_usage();
			throw;
			_exit(~0);
		}
	}

	void _authenticate(ssh_session &session)
	{
		if (ssh_userauth_publickey_auto(session, NULL, NULL) != SSH_AUTH_SUCCESS) {
			if (ssh_userauth_password(session, NULL, _password.string()) != SSH_OK) {
				ssh_userauth_none(session, NULL);
			}
		}
	}

	Main(Libc::Env &env) : _env(env) { }

	void annoy()
	{
		ssh_session session = ssh_new();

		_configure(session);

		if (ssh_connect(session) == SSH_OK)
			_authenticate(session);

		ssh_free(session);
	}
};


static void log_callback(int priority,
                         const char *function,
                         const char *buffer,
                         void *userdata)
{
	(void)userdata;
	(void)function;
	Genode::log(buffer);
}


void Libc::Component::construct(Libc::Env &env)
{
	with_libc([&] () {
		Genode::log("libssh ", ssh_version(0));

		ssh_set_log_callback(log_callback);
		ssh_init();

		static Ssh_client::Main main(env);
		while (true)
			main.annoy();
	});
}
