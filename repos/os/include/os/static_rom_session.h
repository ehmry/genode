/*
 * \brief  ROM session implementation for serving static content
 * \author Emery Hemingway
 * \date   2018-12-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__STATIC_ROM_SESSION_H_
#define _INCLUDE__OS__STATIC_ROM_SESSION_H_

#include <rom_session/rom_session.h>
#include <base/rpc_server.h>
#include <base/attached_ram_dataspace.h>
#include <util/xml_node.h>

namespace Genode { class Static_rom_session; }


class Genode::Static_rom_session : public Rpc_object<Rom_session>
{
	private:

		Rpc_entrypoint         &_ep;
		Attached_ram_dataspace  _ds;

	public:

		/**
		 * Constructor
		 *
		 * \param ep       entrypoint serving the ROM session
		 * \param pd       PD session used to allocate the backing
		 *                 store for the dataspace handed out to the
		 *                 client
		 * \param rm       local region map ('env.rm()') required to
		 *                 make the dataspace locally visible to
		 *                 populate its content
		 *
		 * \param content  XML content of ROM dataspace
		 *
		 * The 'Static_rom_session' associates/disassociates itself with 'ep'.
		 */
		Static_rom_session(Rpc_entrypoint &ep,
		                   Pd_session     &pd,
		                   Region_map     &rm,
		                   Xml_node const &content)
		: Static_rom_session(ep, pd, rm, content.addr(), content.size()) { }

		/**
		 * Constructor
		 *
		 * Low-level constructor for creating a ROM session object
		 * populated using a raw buffer.
		 */
		Static_rom_session(Rpc_entrypoint &ep,
		                   Pd_session     &pd,
		                   Region_map     &rm,
		                   void const     *content,
		                   size_t          size)
		:
			_ep(ep), _ds(pd, rm, size)
		{
			memcpy(_ds.local_addr<void>(), content,
			       min(size, _ds.size()));
			_ep.manage(this);
		}

		~Static_rom_session() { _ep.dissolve(this); }


		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override
		{
			Dataspace_capability ds_cap = _ds.cap();
			return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		bool update() override { return true; }

		void sigh(Signal_context_capability sigh) override
		{
			if (sigh.valid())
				Signal_transmitter(sigh).submit();
		}
};

#endif /* _INCLUDE__OS__STATIC_ROM_SESSION_H_ */
