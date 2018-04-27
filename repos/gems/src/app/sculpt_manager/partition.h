/*
 * \brief  Representation of block-device partition
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PARTITION_H_
#define _PARTITION_H_

#include "types.h"
#include "capacity.h"

namespace Sculpt_manager {

	struct File_system;
	struct Partition;
	struct Partition_update_policy;

	typedef List_model<Partition> Partitions;
};


struct Sculpt_manager::File_system
{
	enum Type { UNKNOWN, EXT2 } type;

	bool accessible() const { return type == EXT2; }

	File_system(Type type) : type(type) { }
};


struct Sculpt_manager::Partition : List_model<Partition>::Element
{
	typedef String<16> Number;
	typedef String<32> Label;

	Number   const number;
	Label    const label;
	Capacity const capacity;

	File_system file_system;

	bool check_in_progress  = false;
	bool format_in_progress = false;

	Partition(Number const &number, Label const &label, Capacity capacity,
	          File_system::Type fs_type)
	:
		number(number), label(label), capacity(capacity), file_system(fs_type)
	{ }
};


/**
 * Policy for transforming a part_blk report into a list of partitions
 */
struct Sculpt_manager::Partition_update_policy : List_model<Partition>::Update_policy
{
	Allocator &_alloc;

	Partition_update_policy(Allocator &alloc) : _alloc(alloc) { }

	void destroy_element(Partition &elem) { destroy(_alloc, &elem); }

	Partition &create_element(Xml_node node)
	{
		auto const file_system = node.attribute_value("file_system", String<16>());
		File_system::Type const fs_type = (file_system == "Ext2")
		                                ? File_system::EXT2 : File_system::UNKNOWN;

		return *new (_alloc)
			Partition(node.attribute_value("number", Partition::Number()),
			          node.attribute_value("name", Partition::Label()),
			          Capacity { node.attribute_value("length",  0ULL)
			                   * node.attribute_value("block_size", 512ULL) },
			          fs_type);
	}

	void update_element(Partition &, Xml_node) { }

	static bool element_matches_xml_node(Partition const &elem, Xml_node node)
	{
		return node.attribute_value("number", Partition::Number()) == elem.number;
	}
};

#endif /* _PARTITION_H_ */
