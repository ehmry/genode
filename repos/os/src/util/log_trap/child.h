/*
 * \brief  Log trap child
 * \author Emery Hemingway
 * \date   2015-08-26
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOG_TRAP__CHILD_H_
#define _LOG_TRAP__CHILD_H_

/* Genode includes */
#include <init/child_config.h>
#include <init/child_policy.h>
#include <cap_session/connection.h>
#include <pd_session/connection.h>
#include <ram_session/connection.h>
#include <rm_session/connection.h>
#include <cpu_session/connection.h>
#include <base/child.h>
#include <base/service.h>
#include <util/string.h>

/* Local includes */
#include "log_policy.h"

namespace Log_trap {

	using namespace Genode;

	class Child;

}

class Log_trap::Child : Genode::Child_policy
{
	private:

		struct Name
		{
			enum { MAX_NAME_LEN = 64 };
			char string[MAX_NAME_LEN];

			Name(Genode::Xml_node start_node)
			{
				try {
					start_node.attribute("name").value(
						string, sizeof(string));
				} catch (Genode::Xml_node::Nonexistent_attribute) {
					PWRN("Missing 'name' attribute in '<start>' entry.\n");
					throw;
				}
			}

		} _name;

		/**
		 * Resources assigned to the child
		 */
		struct Resources
		{
			Genode::Pd_connection  pd;
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;
			Genode::Rm_connection  rm;

			Resources(const char *label)
			:
				pd(label),
				ram(label),
				cpu(label)
			{
				/* keep a small amount of RAM in reserve */
				size_t ram_quota =
					env()->ram_session()->avail() -
					(env()->ram_session()->used()/4);

				ram.ref_account(Genode::env()->ram_session_cap());
				Genode::env()->ram_session()->transfer_quota(
					ram.cap(), ram_quota);
			}
		} _resources;

		Cap_connection _cap_session;

		/*
		 * Entry point used for serving the parent interface and the
		 * locally provided ROM sessions for the 'config' and 'binary'
		 * files.
		 */
		enum { ENTRYPOINT_STACK_SIZE = 5*1024 };
		Genode::Rpc_entrypoint _entrypoint;

		/**
		 * ELF binary
		 */
		Genode::Rom_connection       _binary_rom;
		Genode::Dataspace_capability _binary_rom_ds;

		/**
		 * Private child configuration
		 */
		Init::Child_config _config;

		Genode::Child _child;

		/**
		 * Policy helpers
		 */
		Init::Child_policy_provide_rom_file  _config_policy;
		Init::Child_policy_provide_rom_file  _binary_policy;
		Init::Child_policy_redirect_rom_file _configfile_policy;
		Log_policy                           _log_policy;

		/**
		 * Cache of services from parent.
		 */
		Genode::Service_registry _parent_services;

	public:


		/**
		 * Constructor
		 */
		Child(Genode::Xml_node start_node)
		:
			_name(start_node),
			_resources(_name.string),
			_entrypoint(&_cap_session, ENTRYPOINT_STACK_SIZE,
			            _name.string, false),
			_binary_rom(_name.string, _name.string),
			_binary_rom_ds(_binary_rom.dataspace()),
			_config(_resources.ram.cap(), start_node),
			_child(_binary_rom_ds,
			       _resources.pd.cap(),
			       _resources.ram.cap(),
			       _resources.cpu.cap(),
			       _resources.rm.cap(),
			       &_entrypoint, this),
			_config_policy("config", _config.dataspace(), &_entrypoint),
			_binary_policy("binary", _binary_rom_ds, &_entrypoint),
			_configfile_policy("config", _config.filename()),
			_log_policy(_name.string, _entrypoint)
		{
			/* start execution of child */
			_entrypoint.activate();
		}


		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return _name.string; }

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args) override
		{
			Genode::Service *service = 0;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(service_name, args)))
				return service;

			/* check for binary file request */
			if ((service = _binary_policy.resolve_session_request(service_name, args)))
				return service;

			/* check for LOG request */
			if ((service = _log_policy.resolve_session_request(service_name, args)))
				return service;

			service = _parent_services.find(service_name);
			if (!service) {
				/* Request from parent and cache. */
				service = new (env()->heap()) Parent_service(service_name);
				_parent_services.insert(service);
			}
			return service;
		}

		void filter_session_args(const char *service,
		                         char *args, Genode::size_t args_len) override
		{
			_configfile_policy.filter_session_args(service, args, args_len);
		}

		/**
		 * The child is not allowed to exit successfully.
		 */
		void exit(int exit_value) override {
			env()->parent()->exit(exit_value ? exit_value : ~0); }

};

#endif