/*
 * \brief  Build child and policy
 * \author Emery Hemingway
 * \date   2015-03-13
 */

#ifndef _NICHTS_STORE__BUILDER_H_
#define _NICHTS_STORE__BUILDER_H_

/* Genode includes */
#include <base/env.h>
#include <base/child.h>
#include <base/printf.h>
#include <init/child_policy.h>
#include <cpu_session/connection.h>
#include <ram_session/connection.h>

#include "derivation.h"


using namespace Genode;

namespace Nichts_store {

	class Builder_policy : public Genode::Child_policy
	{

		private:

			char const                          *_name;
			Native_pd_args                       _pd_args;
			Ram_dataspace_capability             _config_ds;
			Init::Child_policy_enforce_labeling  _labeling_policy;
			Init::Child_policy_provide_rom_file  _binary_policy;
			Init::Child_policy_provide_rom_file  _config_policy;
			Init::Child_policy_pd_args           _pd_args_policy;
			Genode::Service_registry            &_parent_services;
			Genode::Signal_context_capability    _success_cap;
			Genode::Signal_context_capability    _failure_cap;
			Genode::Ram_session                 &_ref_ram_session;

		public:

			/**
			 * Constructor
			 */
			Builder_policy(char const                *name,
			               Native_pd_args const      &pd_args,
			               Dataspace_capability       binary_ds,
			               Ram_dataspace_capability   config_ds,
			               Rpc_entrypoint            &entrypoint,
			               Service_registry          &parent_services,
			               Signal_context_capability  success_cap,
			               Signal_context_capability  failure_cap,
			               Ram_session               &ref_ram_session)
			:
				_name(name),
				_pd_args(pd_args),
				_config_ds(config_ds),
				_labeling_policy(name),
				_binary_policy("binary", binary_ds, &entrypoint),
				_config_policy("config", config_ds, &entrypoint),
				_pd_args_policy(&_pd_args),
				_parent_services(parent_services),
				_success_cap(failure_cap),
				_failure_cap(failure_cap),
				_ref_ram_session(ref_ram_session)
			{ }

			char const *name() const { return _name; }
			Native_pd_args const *pd_args() const { return &_pd_args; }

			Genode::Service *resolve_session_request(const char *service_name,
			                                         const char *args)
			{
				Genode::Service *service = 0;

				/* Check for ROM requests that we serve. */
				if ((service = _binary_policy.resolve_session_request(service_name, args))
					|| (service = _config_policy.resolve_session_request(service_name, args)))
					return service;

				/* TODO: Manage a LOG session. */

				/* TODO: enforce a File_system policy. */

				PLOG("requesting session from parent, %s:%s", service_name, args);
				return _parent_services.find(service_name);
			}

			void filter_session_args(const char *service,
			                         char *args, Genode::size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
				_pd_args_policy. filter_session_args(service, args, args_len);
			}

			void exit(int value)
			{
				Signal_transmitter t(value ? _failure_cap : _success_cap);
				t.submit();
			}

			Ram_session *ref_ram_session()
			{
				return &_ref_ram_session;
			}

	};

	class Builder
	{
		private:

			enum { STACK_SIZE = 6*1024*sizeof(long) };

			Rpc_entrypoint            _entrypoint;

			/**
			 * Resources assigned to the child.
			 */
			struct Resources
			{
				Genode::Cpu_connection cpu;
				Genode::Rm_connection  rm;

				Resources(Signal_context_capability sigh)
				{
					/* register default exception handler */
					cpu.exception_handler(Thread_capability(), sigh);

					/* register handler for unresolvable page faults */
					rm.fault_handler(sigh);
				}

			} _resources;

			Builder_policy  _child_policy;
			Genode::Child _child;

		public:

			/**
			 * Constructor
			 */
			Builder(char const                *label,
                    Native_pd_args const      &pd_args,
			        Cap_session               *cap_session,
			        Dataspace_capability       binary_ds,
			        Ram_dataspace_capability   config_ds,
			        Signal_context_capability  success_cap,
			        Signal_context_capability  failure_cap,
			        Service_registry          &parent_services,
			        Ram_connection            &ram,
			        Cpu_connection            &cpu)
				/* TODO: take an affinity location and pass it to the entrypoint. */
			:
				_entrypoint(cap_session, STACK_SIZE, "builder"),
				_resources(failure_cap),
				_child_policy(label, pd_args,
				              binary_ds, config_ds,
				              _entrypoint, parent_services,
				              success_cap, failure_cap,
				              ram),
				_child(binary_ds,
				       ram.cap(),
				       cpu.cap(),
				       _resources.rm.cap(),
				      &_entrypoint,
				      &_child_policy)
			{ }

	};

};

#endif