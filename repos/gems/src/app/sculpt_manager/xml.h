/*
 * \brief  Utilities for generating XML
 * \author Norman Feske
 * \date   2018-01-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _XML_H_
#define _XML_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <base/log.h>

/* local includes */
#include "types.h"

namespace Sculpt_manager {

	template <typename SESSION>
	static inline void gen_parent_service(Xml_generator &xml)
	{
		xml.node("service", [&] () {
			xml.attribute("name", SESSION::service_name()); });
	};

	template <typename SESSION>
	static inline void gen_parent_route(Xml_generator &xml)
	{
		xml.node("service", [&] () {
			xml.attribute("name", SESSION::service_name());
			xml.node("parent", [&] () { });
		});
	}

	static inline void gen_parent_unscoped_rom_route(Xml_generator  &xml,
	                                                 Rom_name const &name)
	{
		xml.node("service", [&] () {
			xml.attribute("name", Rom_session::service_name());
			xml.attribute("unscoped_label", name);
			xml.node("parent", [&] () {
				xml.attribute("label", name); });
		});
	}

	static inline void gen_parent_rom_route(Xml_generator  &xml,
	                                        Rom_name const &name)
	{
		xml.node("service", [&] () {
			xml.attribute("name", Rom_session::service_name());
			xml.attribute("label", name);
			xml.node("parent", [&] () {
				xml.attribute("label", name); });
		});
	}

	static inline void gen_common_start_content(Xml_generator   &xml,
	                                            Rom_name  const &name,
	                                            Cap_quota const  caps,
	                                            Ram_quota const  ram)
	{
		xml.attribute("name", name);
		xml.attribute("caps", caps.value);
		xml.node("resource", [&] () {
			xml.attribute("name", "RAM");
			xml.attribute("quantum", String<64>(Number_of_bytes(ram.value)));
		});
	}
}

#endif /* _XML_H_ */
