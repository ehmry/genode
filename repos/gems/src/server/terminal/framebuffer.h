/*
 * \brief  Terminal framebuffer output backend
 * \author Norman Feske
 * \date   2018-02-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

/* Genode includes */
#include <framebuffer_session/connection.h>

/* local includes */
#include "types.h"

namespace Terminal { class Framebuffer; }

class Terminal::Framebuffer
{
	private:

		typedef Nitpicker::Session::Command Command;

		Env &_env;

		Nitpicker::Connection &_nitpicker;

		::Framebuffer::Session &_fb
			{ *_nitpicker.framebuffer() };

		Constructible<Attached_dataspace> _ds { };

		::Framebuffer::Mode _fb_mode { };

		Nitpicker::Session::View_handle const _view_handle
			{ _nitpicker.create_view() };

	public:

		/**
		 * Constructor
		 */
		Framebuffer(Env &env, Nitpicker::Connection &np)
		: _env(env), _nitpicker(np) { }

		unsigned w() const { return _fb_mode.width(); }
		unsigned h() const { return _fb_mode.height(); }

		template <typename PT>
		PT *pixel() { return _ds->local_addr<PT>(); }

		void refresh(Rect rect)
		{
			_fb.refresh(rect.x1(), rect.y1(), rect.w(), rect.h());
		}

		void switch_to_new_mode(Area area)
		{
			/*
			 * The mode information must be obtained before updating the
			 * dataspace to ensure that the mode is consistent with the
			 * obtained version of the dataspace.
			 *
			 * Otherwise - if the server happens to change the mode just after
			 * the dataspace update - the mode information may correspond to
			 * the next pending mode at the server while we are operating on
			 * the old (possibly too small) dataspace.
			 */
			::Framebuffer::Mode mode(
				area.w(), area.h(),
				::Framebuffer::Mode::RGB565);
			_nitpicker.buffer(mode, false);
			_fb_mode = _fb.mode();
			_ds.construct(_env.rm(), _fb.dataspace());
		}

		void switch_to_new_view(Area area)
		{
			using namespace Nitpicker;
			Area next_area(
				min((unsigned)_fb_mode.width(),  area.w()),
				min((unsigned)_fb_mode.height(), area.h()));

			_nitpicker.enqueue<Command::Geometry>(
				_view_handle, Rect(Point(0,0), next_area));
			_nitpicker.execute();
		}
};

#endif /* _FRAMEBUFFER_H_ */
