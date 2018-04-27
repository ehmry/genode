/*
 * \brief  Representation of USB storage devices
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _USB_STORAGE_DEVICE_H_
#define _USB_STORAGE_DEVICE_H_

#include "types.h"
#include "capacity.h"

namespace Sculpt_manager {

	struct Usb_storage_device;
	struct Usb_storage_device_update_policy;

	typedef List_model<Usb_storage_device> Usb_storage_devices;
};


struct Sculpt_manager::Usb_storage_device : List_model<Usb_storage_device>::Element
{
	typedef String<32> Label;
	Label const label;

	typedef String<28> Vendor;
	typedef String<48> Product;

	struct Driver_info
	{
		Vendor   const vendor;
		Product  const product;
		Capacity const capacity;

		Driver_info(Vendor const &vendor, Product const &product, Capacity capacity)
		: vendor(vendor), product(product), capacity(capacity) { }
	};

	/* information provided asynchronously by usb_block_drv */
	Constructible<Driver_info> driver_info { };

	Partition_table partition_table;

	Attached_rom_dataspace _driver_report_rom;

	void process_driver_report()
	{
		_driver_report_rom.update();

		Xml_node report = _driver_report_rom.xml();

		log("usb_block_drv report: ", report);

		if (!report.has_sub_node("device"))
			return;

		Xml_node device = report.sub_node("device");

		driver_info.construct(device.attribute_value("vendor",  Vendor()),
		                      device.attribute_value("product", Product()),
		                      Capacity { device.attribute_value("block_count", 0ULL)
		                               * device.attribute_value("block_size",  0ULL) });
	}

	bool usb_block_drv_needed() const
	{
		return true; // XXX
	}

	Label usb_block_drv_name() const { return Label(label, ".drv"); }

	Usb_storage_device(Env &env, Allocator &alloc, Signal_context_capability sigh,
	                   Label const &label)
	:
		label(label), partition_table(env, alloc, label, sigh),
		_driver_report_rom(env, String<80>("report -> runtime/", usb_block_drv_name(),
		                                   "/devices").string())
	{
		_driver_report_rom.sigh(sigh);
		process_driver_report();
	}

	inline void gen_usb_block_drv_start_content(Xml_generator &xml) const;
};


void Sculpt_manager::Usb_storage_device::gen_usb_block_drv_start_content(Xml_generator &xml) const
{
	gen_common_start_content(xml, usb_block_drv_name(),
	                         Cap_quota{100}, Ram_quota{4*1024*1024});

	xml.node("binary", [&] () { xml.attribute("name", "usb_block_drv"); });

	xml.node("config", [&] () {
		xml.attribute("report", "yes"); });

	xml.node("provides", [&] () {
		xml.node("service", [&] () {
			xml.attribute("name", Block::Session::service_name()); }); });

	xml.node("route", [&] () {
		gen_parent_unscoped_rom_route(xml, "usb_block_drv");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);

		xml.node("service", [&] () {
			xml.attribute("name", Usb::Session::service_name());
			xml.node("parent", [&] () {
				xml.attribute("label", label); });
		});
		xml.node("service", [&] () {
			xml.attribute("name", Report::Session::service_name());
			xml.node("parent", [&] () { });
		});
	});
}


struct Sculpt_manager::Usb_storage_device_update_policy
:
	List_model<Usb_storage_device>::Update_policy
{
	Env       &_env;
	Allocator &_alloc;

	Signal_context_capability _sigh;

	Usb_storage_device_update_policy(Env &env, Allocator &alloc,
	                                 Signal_context_capability sigh)
	:
		_env(env), _alloc(alloc), _sigh(sigh)
	{ }

	typedef Usb_storage_device::Label Label;

	void destroy_element(Usb_storage_device &elem) { destroy(_alloc, &elem); }

	Usb_storage_device &create_element(Xml_node node)
	{
		return *new (_alloc)
			Usb_storage_device(_env, _alloc, _sigh,
			                   node.attribute_value("label_suffix", Label()));
	}

	void update_element(Usb_storage_device &, Xml_node) { }

	static bool node_is_element(Xml_node node)
	{
		return node.attribute_value("class", String<32>()) == "storage";
	};

	static bool element_matches_xml_node(Usb_storage_device const &elem, Xml_node node)
	{
		return node.attribute_value("label_suffix", Label()) == elem.label;
	}
};

#endif /* _BLOCK_DEVICE_H_ */
