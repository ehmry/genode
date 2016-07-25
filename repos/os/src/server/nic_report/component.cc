/*
 * \brief  Nic session configuration reporter
 * \author Emery Hemingway
 * \date   2016-07-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <report_session/connection.h>
#include <nic_session/capability.h>
#include <rom_session/client.h>
#include <base/attached_dataspace.h>
#include <base/session_label.h>
#include <root/component.h>
#include <base/component.h>
#include <base/log.h>

namespace Nic_report {
	using namespace Genode;
	using namespace Nic;

	class Session_component;
	class Root_component;
	struct Main;
}


class Nic_report::Session_component :
	public Rpc_object<Nic::Session, Nic_report::Session_component>
{
	private:

		struct Nic_backend :
			Genode::Connection<Nic::Session>,
			Genode::Rpc_client<Nic::Session>
		{
			Nic_backend(Parent &parent, char const *args)
			:
				Connection<Session>(session(parent, args)),
				Rpc_client<Session>(cap())
			{ }

			Rom_session_capability state_rom() override {
				return call<Rpc_state_rom>(); }
		};

		class Rom_proxy : public Rpc_object<Rom_session>
		{
			private:

				Rom_session_client _backend;
				Attached_dataspace _rom_ds;

				Report::Connection _report;
				Attached_dataspace _report_ds;

				Signal_context_capability _client_sigh;

				Signal_handler<Rom_proxy> _update_handler;

				/**
				 * Blocks until the dataspace is valid
				 */
				Rom_dataspace_capability _first_valid_ds()
				{
					Rom_dataspace_capability rom_ds =
						_backend.dataspace();

					if (!rom_ds.valid()) {
						Signal_context  sig_ctx;
						Signal_receiver sig_rec;
						_backend.sigh(sig_rec.manage(&sig_ctx));
						do {
							sig_rec.wait_for_signal();
							rom_ds = _backend.dataspace();
						} while (!rom_ds.valid());
						sig_rec.dissolve(&sig_ctx);
					}
					return rom_ds;
				}

				void _copy()
				{
					size_t len = strlen(_rom_ds.local_addr<char const>());
					strncpy(_report_ds.local_addr<char>(),
					        _rom_ds.local_addr<char const>(),
					        len+1);
					_report.submit(len);
				};

				void _update()
				{
					if (_backend.update()) {
						_copy();
						if (_client_sigh.valid())
							Signal_transmitter(_client_sigh).submit();
					}
				}

			public:

				Rom_proxy(Genode::Env            &env,
				          Rom_session_capability  rom_cap,
				          Session_label    const &label)
				:
					_backend(rom_cap),
					_rom_ds(env.rm(), _first_valid_ds()),
					_report(env, label.string(), _rom_ds.size()),
					_report_ds(env.rm(), _report.dataspace()),
					_update_handler(env.ep(), *this, &Rom_proxy::_update)
				{
					_copy();
				}


				/***************************
				 ** ROM session interface **
				 ***************************/

				Rom_dataspace_capability dataspace() override
				{
					Dataspace_capability ds_cap =
						static_cap_cast<Dataspace>(_rom_ds.cap());
					return static_cap_cast<Rom_dataspace>(ds_cap);
				}

				bool update() override { return _backend.update(); }

				void sigh(Signal_context_capability sig_cap) override {
					_client_sigh = sig_cap; }
		};

		Genode::Entrypoint &_ep;

		Nic_backend _backend;
		Rom_proxy   _rom;

		Rom_session_capability _rom_cap;

	public:

		Session_component(Genode::Env &env,
		                  Session_label const &label,
		                  char const *args)
		:
			_ep(env.ep()),
			_backend(env.parent(), args),
			_rom(env, _backend.call<Rpc_state_rom>(), label),
			_rom_cap(_ep.manage(_rom))
		{ }

		~Session_component() { _ep.dissolve(_rom); }


		/***************************
		 ** Nic session interface **
		 ***************************/

		Capability<Tx> _tx_cap() {
			return _backend.call<Rpc_tx_cap>(); }

		Capability<Rx> _rx_cap() {
			return _backend.call<Rpc_rx_cap>(); }

		Rom_session_capability state_rom() override {
			return _rom_cap; }
};


class Nic_report::Root_component :
		public Genode::Root_component<Nic_report::Session_component>
{
	private:

		Genode::Env &_env;

	protected:

		Nic_report::Session_component *_create_session(const char *args) override
		{
			Session_label const label = label_from_args(args);
			Session_component *session = new (md_alloc())
				Session_component(_env, label, args);
			log("reporting Nic state of '", label, "'");
			return session;
		}

	public:

		Root_component(Genode::Env &env, Allocator &md_alloc)
		:
			Genode::Root_component<Nic_report::Session_component>(
				env.ep(), md_alloc),
			_env(env)
		{ }

};

void Component::construct(Genode::Env &env)
{
	static Genode::Heap heap(env.ram(), env.rm());

	static Nic_report::Root_component root(env, heap);

	env.parent().announce(env.ep().manage(root));
}
