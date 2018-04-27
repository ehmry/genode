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

	bool _block_devices_report_valid = false;
	bool _usb_active_config_valid    = false;

	/**
	 * Update 'block_devices' from 'block_devices' report
	 */
	template <typename POLICY>
	void update_block_devices_from_xml(POLICY &policy, Xml_node node)
	{
		block_devices.update_from_xml(policy, node);

		if (node.has_type("block_devices"))
			_block_devices_report_valid = true;
	}

	/**
	 * Update 'usb_storage_devices' from 'usb_active_config' report
	 */
	template <typename POLICY>
	void update_usb_storage_devices_from_xml(POLICY &policy, Xml_node node)
	{
		log("update_usb_storage_devices_from_xml: ", node);
		usb_storage_devices.update_from_xml(policy, node);

		if (node.has_type("raw"))
			_usb_active_config_valid = true;
	}

	/**
	 * Return true as soon as the storage-device information from the drivers
	 * subsystem is complete.
	 */
	bool all_devices_enumerated() const
	{
		log("_block_devices_report_valid=", _block_devices_report_valid, " _usb_active_config_valid=", _usb_active_config_valid);
		return _block_devices_report_valid && _usb_active_config_valid;
	}

	template <typename FN>
	void for_each(FN const &fn) const
	{
		block_devices.for_each([&] (Block_device const &dev) {
			fn(dev); });

		usb_storage_devices.for_each([&] (Usb_storage_device const &dev) {
			fn(dev); });
	}

	template <typename FN>
	void for_each(FN const &fn)
	{
		block_devices.for_each([&] (Block_device &dev) {
			fn(dev); });

		usb_storage_devices.for_each([&] (Usb_storage_device &dev) {
			fn(dev); });
	}
};

#endif /* _STORAGE_DEVICES_ */
