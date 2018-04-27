/*
 * \brief  Common types used within the Sculpt manager
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <util/list_model.h>
#include <base/env.h>
#include <base/attached_rom_dataspace.h>
#include <platform_session/platform_session.h>
#include <nitpicker_session/nitpicker_session.h>
#include <usb_session/usb_session.h>
#include <log_session/log_session.h>
#include <timer_session/timer_session.h>
#include <file_system_session/file_system_session.h>
#include <report_session/report_session.h>
#include <block_session/block_session.h>
#include <rom_session/rom_session.h>

namespace Sculpt_manager {

	using namespace Genode;

	typedef String<32> Rom_name;
}

#endif /* _TYPES_H_ */
