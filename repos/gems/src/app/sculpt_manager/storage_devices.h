/*
 * \brief  Registry of known storage devices
 * \author Norman Feske
 * \date   2018-05-03
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STORAGE_DEVICES_
#define _STORAGE_DEVICES_

#include "types.h"
#include "block_device.h"
#include "usb_storage_device.h"

namespace Sculpt_manager { struct Storage_devices; }

struct Sculpt_manager::Storage_devices
{
	Block_devices       block_devices       { };
	Usb_storage_devices usb_storage_devices { };

	template <typename FN>
	void _for_each_partition_table(FN const &fn) const
	{
		block_devices.for_each([&] (Block_device const &dev) {
			fn(dev.partition_table); });

		usb_storage_devices.for_each([&] (Usb_storage_device const &dev) {
			fn(dev.partition_table); });
	}

	template <typename FN>
	void _for_each_partition_table(FN const &fn)
	{
		block_devices.for_each([&] (Block_device &dev) {
			fn(dev.partition_table); });

		usb_storage_devices.for_each([&] (Usb_storage_device &dev) {
			fn(dev.partition_table); });
	}

	template <typename FN>
	void for_each_partition(FN const &fn) const
	{
		_for_each_partition_table([&] (Partition_table const &partition_table) {
			partition_table.partitions.for_each([&] (Partition const &partition) {
				fn(partition_table._label, partition); }); });
	}

	template <typename FN>
	void for_each_partition(FN const &fn)
	{
		_for_each_partition_table([&] (Partition_table &partition_table) {
			partition_table.partitions.for_each([&] (Partition &partition) {
				fn(partition_table._label, partition); }); });
	}
};

#endif /* _STORAGE_DEVICES_ */
