/*
 * \brief  Utility to measure component lifetime
 * \author Emery Hemingway
 * \date   2018-01-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <timer_session/connection.h>
#include <init/child_policy.h>
#include <base/attached_rom_dataspace.h>
#include <os/child_policy_dynamic_rom.h>
#include <base/sleep.h>
#include <base/child.h>
#include <base/component.h>

namespace Time {
	using namespace Genode;

	struct Child;
	struct Main;
}


struct Time::Child : Genode::Child_policy
{
	Genode::Env &_env;

	Timer::Connection _timer { _env, "time" };

	Heap _services_heap { _env.pd(), _env.rm() };

	Attached_rom_dataspace _config_rom { _env, "config" };

	Xml_node const _start_node = _config_rom.xml().sub_node("start");

	Name const _name = _start_node.attribute_value("name", Name());

	Binary_name _start_binary()
	{
		Binary_name name;
		try {
			_start_node.sub_node("binary").attribute("name").value(&name);
			return name != "" ? name : _name;
		}
		catch (...) { return _name; }
	}

	Binary_name const _binary_name = _start_binary();

	Child_policy_dynamic_rom_file _config_policy {
		_env.rm(), "config", _env.ep().rpc_ep(), &_env.pd() };

	class Parent_service : public Genode::Parent_service
	{
		private:

			Registry<Parent_service>::Element _reg_elem;

		public:

			Parent_service(Registry<Parent_service> &registry, Env &env,
			               Service::Name const &name)
			:
				Genode::Parent_service(env, name), _reg_elem(registry, *this)
			{ }
	};

	Registry<Parent_service> _parent_services { };

	Genode::Child _child { _env.rm(), _env.ep().rpc_ep(), *this };

	bool const _have_config = _start_node.has_sub_node("config");

	unsigned long _start_ms = 0;

	Child(Genode::Env &env) : _env(env)
	{
		if (_have_config) {
			Xml_node config_node = _start_node.sub_node("config");
			_config_policy.load(config_node.addr(), config_node.size());
		}
	}

	~Child()
	{
		_parent_services.for_each([&] (Parent_service &service) {
			destroy(_services_heap, &service); });
	}


	/****************************
	 ** Child_policy interface **
	 ****************************/

	Name name() const override { return _name; }

	Binary_name binary_name() const override { return _binary_name; }

	/**
	 * Provide a "config" ROM if configured to do so,
	 * otherwise forward directly to the parent.
	 */
	Service &resolve_session_request(Service::Name       const &name,
	                                 Session_state::Args const &args)
	{
		if (_have_config) {
			Service *s = _config_policy.resolve_session_request(
				name.string(), args.string());
			if (s)
				return *s;
		}

		return *new (_services_heap) Parent_service(_parent_services, _env, name);
	}

	Pd_session           &ref_pd()           override { return _env.pd(); }
	Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

	/**
	 * Log the measured lifetime and exit
	 */
	void exit(int exit_value) override
	{
		unsigned long stop_ms = _timer.elapsed_ms();

		log("child \"", name(), "\" exited with exit value ", exit_value);

		double elapsed_s = (stop_ms- _start_ms) / 1000.0;
		log("\"", name(), "\" was active for ", elapsed_s, " seconds");

		_env.parent().exit(exit_value);
	}

	void resource_request(Parent::Resource_args const &args) override
	{
		Ram_quota ram = ram_quota_from_args(args.string());
		Cap_quota caps = cap_quota_from_args(args.string());

		Pd_session_capability pd_cap = _child.pd_session_cap();

		if (ram.value) {
			Ram_quota avail = _env.pd().avail_ram();
			if (avail.value > ram.value) {
				ref_pd().transfer_quota(pd_cap, ram);
			} else {
				ref_pd().transfer_quota(pd_cap, Ram_quota{avail.value-4096});
				_env.parent().resource_request(args);
			}
		}

		if (caps.value) {
			Cap_quota avail = _env.pd().avail_caps();
			if (avail.value > caps.value) {
				ref_pd().transfer_quota(pd_cap, caps);
			} else {
				ref_pd().transfer_quota(pd_cap, Cap_quota{avail.value-4});
				_env.parent().resource_request(args);
			}
		}

		_child.notify_resource_avail();
	}

	/**
	 * Begin measurement as the PD session is initialized
	 */
	void init(Pd_session &pd, Pd_session_capability pd_cap) override
	{
		pd.ref_account(ref_pd_cap());
		ref_pd().transfer_quota(
			pd_cap, Cap_quota{_env.pd().avail_caps().value >> 1});
		ref_pd().transfer_quota(
			pd_cap, Ram_quota{_env.pd().avail_ram().value >> 1});

		/* child entrypoint will start soon hereafter */
		_start_ms = _timer.elapsed_ms();
	}
};


void Component::construct(Genode::Env &env) {
	static Time::Child child(env); }
