/*
 * \brief  Ethernet switch service
 * \author Emery Hemingway
 * \date   2019-04-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "session_component.h"
#include "mac_allocator.h"

/* Genode */
#include <net/mac_address.h>
#include <root/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/session_policy.h>
#include <base/session_label.h>
#include <base/env.h>
#include <base/log.h>
#include <base/component.h>

namespace Nic_switch {
	using namespace Net;
	using namespace Genode;
	class Root;
	struct Main; 
}


class Nic_switch::Root : public Genode::Root_component<Nic_switch::Session_component>
{
	private:

		Genode::Env            &_env;
		Attached_rom_dataspace  _config_rom  { _env, "config" };
		Interface_registry      _interface_registry { };
		Net::Mac_address        _proxy_mac { };
		Mac_allocator           _mac_alloc { Mac_address() };

		void _handle_config()
		{
			_proxy_mac = _config_rom.xml()
				.attribute_value("mac", _proxy_mac);
			Genode::log("proxy MAC is ", _proxy_mac);
		}

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Session_label  const label  { label_from_args(args) };
			Session_policy const policy { label, _config_rom.xml() };

			Mac_address mac { policy.attribute_value("mac", Mac_address()) };

			if (mac == Mac_address() || mac == _proxy_mac) {
				try { mac = _mac_alloc.alloc(); }
				catch (Mac_allocator::Alloc_failed) {
					Genode::warning("MAC address allocation failed!");
					throw Service_denied();
				}
			} else if (_mac_alloc.mac_managed_by_allocator(mac)) {
				Genode::warning("MAC address already in use");
				throw Service_denied();
			}

			auto *session =  new (md_alloc())
				Session_component(_env.ep(), _env.ram(), _env.rm(),
				                  ram_quota_from_args(args),
				                  cap_quota_from_args(args),
				                  Tx_size{Arg_string::find_arg(args, "tx_buf_size").ulong_value(0)},
				                  Rx_size{Arg_string::find_arg(args, "rx_buf_size").ulong_value(0)},
				                  _interface_registry,
				                  label, mac);
			Genode::log(mac, " assigned to ", label);
			return session;
		}

	public:

		Root(Genode::Env        &env,
		     Genode::Allocator  &md_alloc)
		:
			Genode::Root_component<Nic_switch::Session_component>(
				env.ep(), md_alloc),
			_env(env)
		{
			_proxy_mac.addr[1] = 3;
			_handle_config();
		}
};


struct Nic_switch::Main
{
	Sliced_heap _sliced_heap;
	Nic_switch::Root _root;

	Main(Genode::Env &env)
	:
		_sliced_heap(env.ram(), env.rm()),
		_root(env, _sliced_heap)
	{
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Nic_switch::Main nic_switch(env); }
