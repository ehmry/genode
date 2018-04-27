/*
 * \brief  Representation of block devices
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BLOCK_DEVICE_H_
#define _BLOCK_DEVICE_H_

#include "capacity.h"
#include "partition_table.h"

namespace Sculpt_manager {

	struct Block_device;
	struct Block_device_update_policy;

	typedef List_model<Block_device> Block_devices;
};


struct Sculpt_manager::Block_device : List_model<Block_device>::Element
{
	typedef String<32> Label;
	typedef String<16> Model;

	Label    const label;
	Model    const model;
	Capacity const capacity;

	Partition_table partition_table;

	Block_device(Env &env, Allocator &alloc, Signal_context_capability sigh,
	             Label const &label, Model const &model, Capacity capacity)
	:
		label(label), model(model), capacity(capacity),
		partition_table(env, alloc, label, sigh)
	{ }
};


struct Sculpt_manager::Block_device_update_policy : List_model<Block_device>::Update_policy
{
	Env       &_env;
	Allocator &_alloc;

	Signal_context_capability _sigh;

	Block_device_update_policy(Env &env, Allocator &alloc, Signal_context_capability sigh)
	: _env(env), _alloc(alloc), _sigh(sigh) { }

	void destroy_element(Block_device &elem) { destroy(_alloc, &elem); }

	Block_device &create_element(Xml_node node)
	{
		return *new (_alloc)
			Block_device(_env, _alloc, _sigh,
			             node.attribute_value("label", Block_device::Label()),
			             node.attribute_value("model", Block_device::Model()),
			             Capacity { node.attribute_value("block_size",  0ULL)
			                      * node.attribute_value("block_count", 0ULL) });
	}

	void update_element(Block_device &, Xml_node) { }

	static bool element_matches_xml_node(Block_device const &elem, Xml_node node)
	{
		return node.attribute_value("label", Block_device::Label()) == elem.label;
	}
};

#endif /* _BLOCK_DEVICE_H_ */
