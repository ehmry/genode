/*
 * \brief  Block root proxy for partition server
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PART_BLK__PROXY_H_
#define _PART_BLK__PROXY_H_

#include <os/session_policy.h>
#include <base/attached_rom_dataspace.h>
#include <base/service.h>
#include <root/component.h>
#include <block/component.h>

namespace Block { class Proxy; };


class Block::Proxy : public Genode::Rpc_object<Genode::Typed_root<Block::Session>>
{
	private:

		Genode::Parent_service          _parent_block { "Block" };
		Genode::Attached_rom_dataspace &_config_rom;
		Partition_table                &_table;

	public:

		Proxy(Genode::Attached_rom_dataspace &config_rom,
		      Partition_table &table)
		: _config_rom(config_rom), _table(table) { }

		Genode::Session_capability session(Session_args const &session_args,
		                                   Genode::Affinity const &affinity) override
		{
			using namespace Genode;

			char const *args = session_args.string();
			long num = -1;
			Block::Policy block_policy;

			/* lookup the partition policy */
			Session_label const label = label_from_args(args);
			try {
				Session_policy policy(label, _config_rom.xml());

				/* read partition attribute */
				policy.attribute("partition").value(&num);

				block_policy = Block::Policy(policy, args);

			} catch (Xml_node::Nonexistent_attribute) {
				error("policy does not define partition number for for '", label, "'");
				throw Root::Unavailable();
			} catch (Session_policy::No_policy_defined) {
				error("rejecting session request, no matching policy for '", label, "'");
				throw Root::Unavailable();
			}

			/* lookup the partion */
			Partition *partition = _table.partition(num);
			if (!partition) {
				error("partition ", num, " unavailable for '", label, "'");
				throw Root::Unavailable();
			}

			/* calculate new constrained arguments */
			if (block_policy.offset >= partition->sectors) {
				error("client requests offset beyond partition end, denying '", label, "'"); 
				throw Root::Unavailable();
			}
			block_policy.offset = partition->lba + block_policy.offset;

			if (block_policy.span > partition->sectors - block_policy.offset) {
				error("client requests span beyond partition end, denying '", label, "'"); 
				throw Root::Unavailable();
			}
			block_policy.span = block_policy.span ? block_policy.span : partition->sectors;

			/* set new arguments */
			enum { ARGS_MAX_LEN = 256 };
			char new_args[ARGS_MAX_LEN];

			strncpy(new_args, args, ARGS_MAX_LEN);
			Arg_string::set_arg(new_args, ARGS_MAX_LEN, "readable",  block_policy.readable);
			Arg_string::set_arg(new_args, ARGS_MAX_LEN, "writeable", block_policy.writeable);
			Arg_string::set_arg(new_args, ARGS_MAX_LEN, "offset",    block_policy.offset);
			Arg_string::set_arg(new_args, ARGS_MAX_LEN, "span",      block_policy.span);

			return _parent_block.session(new_args, affinity);
		}
	
		void upgrade(Genode::Session_capability session, Upgrade_args const &args) {
			_parent_block.upgrade(session, args.string()); }
	
		void close(Genode::Session_capability session) override {
			_parent_block.close(session); }

};

#endif /* _PART_BLK__PROXY_H_ */
