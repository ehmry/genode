/*
 * \brief  Utility to log all RAM session allocations
 * \author Emery Hemingway
 * \date   2019-03-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <init/child_policy.h>
#include <base/attached_rom_dataspace.h>
#include <os/child_policy_dynamic_rom.h>
#include <timer_session/connection.h>
#include <cpu_session/connection.h>
#include <pd_session/connection.h>
#include <base/rpc_server.h>
#include <base/sleep.h>
#include <base/child.h>
#include <base/component.h>

namespace Alloc_logger {
	using namespace Genode;
	struct Child;
	struct Pd_session_component;
	struct Cpu_session_component;
	typedef Local_service<Pd_session_component>  Pd_service;
	typedef Local_service<Cpu_session_component> Cpu_service;
}


struct Alloc_logger::Pd_session_component : Rpc_object<Pd_session>
{
	Rpc_entrypoint &_ep;

	Timer::Connection &_timer;

	Pd_connection _pd;

	Pd_session_component(Rpc_entrypoint &ep,
	                     Genode::Env &env,
	                     Timer::Connection &timer,
	                     Child_policy::Name const &label)
	: _ep(ep), _timer(timer), _pd(env, label.string())
	{
		_ep.manage(this);
	}

	~Pd_session_component() {
		_ep.dissolve(this); }

	Pd_session_capability core_pd_cap() { return _pd.cap(); }

	size_t _alloced = 0;


	/*****************************
	 ** Ram_allocator interface **
	 *****************************/

	Ram_dataspace_capability alloc(size_t size,
	                               Cache_attribute cached) override
	{
		_alloced += size;
		log(_timer.elapsed_ms(), " ", _alloced);
		return _pd.alloc(size, cached);
	}

	void free(Ram_dataspace_capability ds) override
	{
		_alloced -= _pd.dataspace_size(ds);
		log(_timer.elapsed_ms(), " ", _alloced);
		_pd.free(ds);
	}

	size_t dataspace_size(Ram_dataspace_capability ds) const override
	{
		return _pd.dataspace_size(ds);
	}

	/*********************************
	 ** Protection domain interface **
	 *********************************/

	void assign_parent(Capability<Parent> cap) override {
		_pd.assign_parent(cap); }

	bool assign_pci(addr_t pci_config_memory_address, uint16_t bdf) override {
		return _pd.assign_pci(pci_config_memory_address, bdf); }

	void map(addr_t virt, addr_t size) override {
		_pd.map(virt, size); }

	Signal_source_capability alloc_signal_source() override {
		return _pd.alloc_signal_source(); }

	void free_signal_source(Signal_source_capability cap) override {
		_pd.free_signal_source(cap); }

	Capability<Signal_context>
	alloc_context(Signal_source_capability source, unsigned long imprint) override {
		return _pd.alloc_context(source, imprint); }

	void free_context(Capability<Signal_context> cap) override {
		return _pd.free_context(cap); }

	void submit(Capability<Signal_context> context, unsigned cnt) override {
		_pd.submit(context, cnt); }

	Native_capability alloc_rpc_cap(Native_capability ep) override {
		return _pd.alloc_rpc_cap(ep); }

	void free_rpc_cap(Native_capability cap) override {
		_pd.free_rpc_cap(cap); }

	Capability<Region_map> address_space() override {
		return _pd.address_space(); }

	Capability<Region_map> stack_area() override {
		return _pd.stack_area(); }

	Capability<Region_map> linker_area() override {
		return _pd.linker_area(); }

	void ref_account(Pd_session_capability cap) override {
		_pd.ref_account(cap); }

	void transfer_quota(Pd_session_capability to, Cap_quota amount) override {
		_pd.transfer_quota(to, amount); }

	Cap_quota cap_quota() const override {
		return _pd.cap_quota(); }

	Cap_quota used_caps() const override {
		return _pd.used_caps(); }

	void transfer_quota(Capability<Pd_session> to, Ram_quota amount) override {
		return _pd.transfer_quota(to, amount); }

	Ram_quota ram_quota() const override {
		return _pd.ram_quota(); }

	Ram_quota used_ram() const override {
		return _pd.used_ram(); }

	Capability<Native_pd> native_pd() override {
		return _pd.native_pd(); }
};


struct Alloc_logger::Cpu_session_component : Rpc_object<Cpu_session>
{
	Rpc_entrypoint &_ep;

	Pd_session_capability _core_pd;

	Cpu_connection _cpu;

	Cpu_session_component(Rpc_entrypoint &ep,
	                      Pd_session_capability core_pd,
	                      Genode::Env &env,
	                      Child_policy::Name const &label)
	: _ep(ep), _core_pd(core_pd), _cpu(env, label.string())
	{
		_ep.manage(this);
	}

	~Cpu_session_component() {
		_ep.dissolve(this); }

	Thread_capability create_thread(Pd_session_capability pd,
	                                Name const            &name,
	                                Affinity::Location     affinity,
	                                Weight                 weight,
	                                addr_t                 utcb) override
	{
		(void)pd;
		return _cpu.create_thread(_core_pd, name, affinity, weight, utcb);
	}

	void kill_thread(Thread_capability thread) override {
		return _cpu.kill_thread(thread); }

	void exception_sigh(Signal_context_capability cap) override {
		return _cpu.exception_sigh(cap); }

	Affinity::Space affinity_space() const override {
		return _cpu.affinity_space(); }

	Dataspace_capability trace_control() override {
		return _cpu.trace_control(); }

	int ref_account(Cpu_session_capability cap) override {
		return _cpu.ref_account(cap); }

	int transfer_quota(Cpu_session_capability cap,
	                   size_t amount) override {
		return _cpu.transfer_quota(cap, amount); }

	Quota quota() {
		return _cpu.quota(); }

	Capability<Native_cpu> native_cpu() override {
		return _cpu.native_cpu(); }

};


struct Alloc_logger::Child : Genode::Child_policy
{
	Genode::Env &_env;

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

	Timer::Connection _timer { _env };

	Pd_session_capability  _ref_pd_cap { _env.pd_session_cap() };
	Pd_session            &_ref_pd     { _env.pd() };

	Pd_session_component _local_pd {
		_env.ep().rpc_ep(), _env, _timer, _name };

	Pd_service::Single_session_factory  _pd_factory { _local_pd };
	Pd_service                          _pd_service { _pd_factory };

	Cpu_session_component _local_cpu {
		_env.ep().rpc_ep(), _local_pd.core_pd_cap(), _env, _name };

	Cpu_service::Single_session_factory  _cpu_factory { _local_cpu };
	Cpu_service                          _cpu_service { _cpu_factory };

	Heap _heap { _env.pd(), _env.rm() };

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

	bool _have_config = _start_node.has_sub_node("config");

	Child(Genode::Env &env) : _env(env)
	{
		if (_have_config) {
			Xml_node config_node = _start_node.sub_node("config");
			_config_policy.load(config_node.addr(), config_node.size());
		}
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
		if (name == "PD")
			return _pd_service;

		if (name == "CPU")
			return _cpu_service;

		if (_have_config) {
			Service *s = _config_policy.resolve_session_request(
				name.string(), args.string());
			if (s)
				return *s;
		}

		Service &s = *new (_heap) Parent_service(_parent_services, _env, name);
		return s;
	}

	Pd_session           &ref_pd()           override { return _ref_pd; }
	Pd_session_capability ref_pd_cap() const override { return _ref_pd_cap; }

	/**
	 * If the exit is successful then queue a child reload via a signal,
	 * otherwise exit this parent component.
	 */
	void exit(int exit_value) override
	{
		_parent_services.for_each([&] (Parent_service &service) {
			destroy(_heap, &service); });

		_env.parent().exit(exit_value);
		sleep_forever();
	}

	/**
	 * Upgrade child quotas from our quotas,
	 * otherwise request more quota from our parent.
	 */
	void resource_request(Parent::Resource_args const &args) override
	{
		(void)args;
		/*
		Ram_quota ram = ram_quota_from_args(args.string());
		Cap_quota caps = cap_quota_from_args(args.string());

		_child.notify_resource_avail();
		*/
	}

	/**
	 * Initialize the child Protection Domain session with half of
	 * the initial quotas of this parent component.
	 */
	void init(Pd_session &session, Pd_session_capability cap) override
	{
		session.ref_account(_ref_pd_cap);

		_env.ep().rpc_ep().apply(cap, [&] (Pd_session_component *pd) {
			if (pd) {
				_ref_pd.transfer_quota(
					pd->core_pd_cap(), Cap_quota{_env.pd().avail_caps().value - 32});
				_ref_pd.transfer_quota(
					pd->core_pd_cap(), Ram_quota{_env.pd().avail_ram().value -  (1 << 18)});
			}
		});
	}
};


void Component::construct(Genode::Env &env) {
	static Alloc_logger::Child child(env); }
