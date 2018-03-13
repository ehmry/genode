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
#include <root/component.h>
#include <base/rpc_server.h>
#include <base/sleep.h>
#include <base/child.h>
#include <base/component.h>

namespace Alloc_logger {
	using namespace Genode;
	struct Child;
	class Pd_session_component;
	class Cpu_session_component;
	class Pd_root;
	struct Session_pair;
	struct Main;

	typedef Local_service<Pd_session_component>  Pd_service;
	typedef Local_service<Cpu_session_component> Cpu_service;
}


class Alloc_logger::Pd_session_component : public Rpc_object<Pd_session>
{
	/*
	 * TODO: log to a per-session log session
	 */

	private:

		Pd_connection &_pd;

		Timer::Connection &_timer;

		size_t _alloced = 0;

	public:

		Pd_session_component(Pd_connection &pd,
		                     Timer::Connection &timer)
		: _pd(pd), _timer(timer) { }


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


class Alloc_logger::Cpu_session_component : public Rpc_object<Cpu_session>
{
	private:

		Cpu_connection _cpu;

		Pd_session_capability _core_pd;

	public:

		Cpu_session_component(Pd_session_capability core_pd,
		                      Genode::Env &env,
		                      Session_label const &label)
		: _cpu(env, label.string()), _core_pd(core_pd)
		{ }

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


struct Alloc_logger::Session_pair : Registry<Alloc_logger::Session_pair>::Element
{
	Genode::Env &env;

	Session_label const label;

	Pd_connection parent_pd { env, label.string() };

	Pd_session_component   pd_session;
	Cpu_session_component cpu_session;

	Pd_session_capability   pd_cap = env.ep().manage( pd_session);
	Cpu_session_capability cpu_cap = env.ep().manage(cpu_session);

	Session_pair(Registry<Alloc_logger::Session_pair> &registry,
	             Genode::Env &env,
	             Timer::Connection &timer,
	             Session_label const &label)
	: Registry<Alloc_logger::Session_pair>::Element(registry, *this),
	  env(env), label(label),
	   pd_session(parent_pd, timer),
	  cpu_session(parent_pd.cap(), env, label)
	{ }

	~Session_pair()
	{
		env.ep().dissolve(pd_session);
		env.ep().dissolve(cpu_session);
	}
};



struct Alloc_logger::Main
{
	Genode::Env &env;

	Genode::Heap heap { env.pd(), env.rm() };

	Registry<Alloc_logger::Session_pair> session_registry { };

	Id_space<Parent::Server> session_id_space { };

	Attached_rom_dataspace session_requests { env, "session_requests" };

	Timer::Connection timer { env, "wall" };

	void create_session(Xml_node request);

	void handle_session_requests(Xml_node requests)
	{
		requests.for_each_sub_node("create", [&] (Xml_node node) {
			create_session(node); });
	}

	void handle_session_requests()
	{
		session_requests.update();
		handle_session_requests(session_requests.xml());
	}

	Signal_handler<Main> session_request_handler {
		env.ep(), *this, &Main::handle_session_requests };

	Main(Genode::Env &env) : env(env)
	{
		log(session_requests.xml());
		handle_session_requests(session_requests.xml());
	}
};


void Alloc_logger::Main::create_session(Xml_node request)
{
	log(request);
	if (!request.has_attribute("id"))
		return;

	Parent::Server::Id const server_id { request.attribute_value("id", 0UL) };

	enum Service_type { PD, CPU, INVALID };

	Service_type service_type = INVALID;

	if (!request.has_sub_node("args"))
		return;

	{
		auto const service_name = request.attribute_value("service", Service::Name());
		log("request for ", service_name);
		if (service_name == "PD")
			service_type = PD;
		else
		if (service_name == "CPU")
			service_type = CPU;
	}

	if (service_type == INVALID) {
		env.parent().session_response(server_id, Parent::SERVICE_DENIED);
		return;
	}

	typedef Session_state::Args Args;
	Args const args = request.sub_node("args").decoded_content<Args>();

	Session_label const label = label_from_args(args.string());
	Session_pair *session = nullptr;

	session_registry.for_each([&] (Session_pair &other) {
		if (session == nullptr && other.label == label)
			session = &other;
	});

	if (session == nullptr) {
		Genode::log("create new session pair");
		session = new (heap)
			Session_pair(session_registry, env, timer, label);
	}

	switch (service_type) {
	case PD:
		log("deliver PD cap");
		env.parent().deliver_session_cap(server_id, session->pd_cap);
		break;
	case CPU:
		log("deliver CPU cap");
		env.parent().deliver_session_cap(server_id, session->cpu_cap);
		break;
	case INVALID: break;
	}
}


void Component::construct(Genode::Env &env)
{
	static Alloc_logger::Main inst(env);

	env.parent().announce("PD");
	env.parent().announce("CPU");
}
