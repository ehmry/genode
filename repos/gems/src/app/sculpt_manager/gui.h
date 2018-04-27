/*
 * \brief  Sculpt GUI management
 * \author Norman Feske
 * \date   2018-04-30
 *
 * The GUI is based on a dynamically configured init component, which hosts
 * one menu-view component for each dialog.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GUI_H_
#define _GUI_H_

#include "types.h"
#include "xml.h"

namespace Sculpt_manager { struct Gui; }

struct Sculpt_manager::Gui
{
	Env &_env;

	Expanding_reporter _config { _env, "config", "gui_config" };

	void _gen_storage_view_start_content(Xml_generator &xml) const;

	void _generate_config(Xml_generator &) const;

	void generate_config()
	{
		_config.generate([&] (Xml_generator &xml) { _generate_config(xml); });
	}

	Gui(Env &env) : _env(env) { }
};


void Sculpt_manager::Gui::_gen_storage_view_start_content(Xml_generator &xml) const
{
	gen_common_start_content(xml, "storage_view",
	                         Cap_quota{150}, Ram_quota{4*1024*1024});

	xml.node("binary", [&] () { xml.attribute("name", "menu_view"); });

	xml.node("config", [&] () {
		xml.node("libc", [&] () { xml.attribute("stderr", "/dev/log"); });
		xml.node("report", [&] () { xml.attribute("hover", "yes"); });
		xml.node("vfs", [&] () {
			xml.node("tar", [&] () {
				xml.attribute("name", "menu_view_styles.tar"); });
			xml.node("dir", [&] () {
				xml.attribute("name", "fonts");
				xml.node("fs", [&] () {
					xml.attribute("label", "fonts"); });
			});
			xml.node("dir", [&] () {
				xml.attribute("name", "dev");
				xml.node("log",  [&] () { });
			});
		});
	});

	xml.node("route", [&] () {
		gen_parent_unscoped_rom_route(xml, "menu_view");
		gen_parent_unscoped_rom_route(xml, "ld.lib.so");
		gen_parent_rom_route(xml, "libc.lib.so");
		gen_parent_rom_route(xml, "libm.lib.so");
		gen_parent_rom_route(xml, "libpng.lib.so");
		gen_parent_rom_route(xml, "zlib.lib.so");
		gen_parent_rom_route(xml, "menu_view_styles.tar");
		gen_parent_route<Cpu_session>         (xml);
		gen_parent_route<Pd_session>          (xml);
		gen_parent_route<Log_session>         (xml);
		gen_parent_route<Timer::Session>      (xml);
		gen_parent_route<Nitpicker::Session>  (xml);
		xml.node("service", [&] () {
			xml.attribute("name", Rom_session::service_name());
			xml.attribute("label", "dialog");
			xml.node("parent", [&] () { });
		});
		xml.node("service", [&] () {
			xml.attribute("name", Report::Session::service_name());
			xml.attribute("label", "hover");
			xml.node("parent", [&] () { });
		});
		xml.node("service", [&] () {
			xml.attribute("name", ::File_system::Session::service_name());
			xml.attribute("label", "fonts");
			xml.node("parent", [&] () {
				xml.attribute("label", "fonts"); });
		});
	});
}


void Sculpt_manager::Gui::_generate_config(Xml_generator &xml) const
{
	xml.node("parent-provides", [&] () {
		gen_parent_service<Rom_session>(xml);
		gen_parent_service<Cpu_session>(xml);
		gen_parent_service<Pd_session>(xml);
		gen_parent_service<Log_session>(xml);
		gen_parent_service<Timer::Session>(xml);
		gen_parent_service<Report::Session>(xml);
		gen_parent_service<Nitpicker::Session>(xml);
		gen_parent_service<::File_system::Session>(xml);
	});

	xml.node("start", [&] () {
		_gen_storage_view_start_content(xml); });
}

#endif /* _GUI_H_ */
