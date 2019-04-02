/*
 * \brief  Terminal service
 * \author Norman Feske
 * \date   2011-08-11
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <framebuffer_session/connection.h>
#include <input_session/connection.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <input/event.h>
#include <nitpicker_session/connection.h>
#include <gems/vfs.h>
#include <gems/vfs_font.h>
#include <gems/cached_font.h>

/* terminal includes */
#include <terminal/decoder.h>
#include <terminal/types.h>

/* local includes */
#include "text_screen_surface.h"
#include "session.h"

namespace Terminal { struct Main; }


struct Terminal::Main : Character_consumer
{
	/*
	 * Noncopyable
	 */
	Main(Main const &);
	Main &operator = (Main const &);

	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Heap _heap { _env.ram(), _env.rm() };

	Root_directory _root_dir { _env, _heap, _config.xml().sub_node("vfs") };

	Cached_font::Limit _font_cache_limit { 0 };

	struct Font
	{
		Vfs_font    _vfs_font;
		Cached_font _cached_font;

		Font(Allocator &alloc, Directory &root_dir, Cached_font::Limit limit)
		:
			_vfs_font(alloc, root_dir, "fonts/monospace/regular"),
			_cached_font(alloc, _vfs_font, limit)
		{ }

		Text_painter::Font const &font() const { return _cached_font; }
	};

	Constructible<Font> _font { };

	Color_palette _color_palette { };

	/*
	 * Adapt terminal to font or framebuffer mode changes
	 */
	void _adjust_geometry();

	void _handle_config();

	void _handle_mode();

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Signal_handler<Main> _mode_handler {
		_env.ep(), *this, &Main::_handle_mode };

	Timer::Connection _timer { _env };

	Nitpicker::Connection _nitpicker { _env };

	Framebuffer _framebuffer { _env, _nitpicker };

	typedef Pixel_rgb565 PT;

	Constructible<Text_screen_surface<PT>> _text_screen_surface { };

	Area _terminal_size { };

	/*
	 * Time in milliseconds between a change of the terminal content and the
	 * update of the pixels. By delaying the update, multiple intermediate
	 * changes result in only one rendering step.
	 */
	unsigned const _flush_delay = 5;

	bool _flush_scheduled = false;

	void _handle_flush()
	{
		_flush_scheduled = false;

		if (_text_screen_surface.constructed())
			_text_screen_surface->redraw();
	}

	Signal_handler<Main> _flush_handler {
		_env.ep(), *this, &Main::_handle_flush };

	void _schedule_flush()
	{
		if (!_flush_scheduled) {
			_timer.trigger_once(1000*_flush_delay);
			_flush_scheduled = true;
		}
	}

	/**
	 * Character_consumer interface, called from 'Terminal::Session_component'
	 */
	void consume_character(Character c) override
	{
		// XXX distinguish between normal and alternative display mode (smcup)
		if (_text_screen_surface.constructed())
			_text_screen_surface->apply_character(c);

		_schedule_flush();
	}

	/* input read buffer */
	Read_buffer _read_buffer { };

	/* create root interface for service */
	Root_component _root { _env, _heap, _read_buffer, *this };

	void _handle_input();

	Signal_handler<Main> _input_handler {
		_env.ep(), *this, &Main::_handle_input };

	Main(Env &env) : _env(env)
	{
		_timer .sigh(_flush_handler);
		_config.sigh(_config_handler);
		_handle_config();

		_nitpicker.input()->sigh(_input_handler);
		_nitpicker.mode_sigh(_mode_handler);

		/* announce service at our parent */
		_env.parent().announce(_env.ep().manage(_root));
	}
};

void Terminal::Main::_adjust_geometry()
{
	/*
	 * Distinguish the case where the framebuffer change affects the character
	 * grid size from the case where merely the pixel position of the character
	 * grid within the framebuffer changed.
	 *
	 * In the former case, the text-screen surface is reallocated and cleared.
	 * Clients (like ncurses) are expected to respond to a terminal-size change
	 * with a redraw. In the latter case, the client would skip the redraw. So
	 * we need to preserve the content and just reposition the character grid.
	 */

	try {
		Text_screen_surface<PT>::Geometry const new_geometry(_font->font(), _framebuffer);

		bool const reconstruct = !_text_screen_surface.constructed() ||
		                          _text_screen_surface->size() != new_geometry.size();
		if (reconstruct) {

			typedef Text_screen_surface<PT>::Snapshot Snapshot;
			Constructible<Snapshot> snapshot { };

			size_t const snapshot_bytes  = _text_screen_surface.constructed()
			                             ? Snapshot::bytes_needed(*_text_screen_surface)
			                             : 0,
			             preserved_bytes = 32*1024,
			             needed_bytes    = snapshot_bytes + preserved_bytes,
			             avail_bytes     = _env.pd().avail_ram().value;

			bool const preserve_content = (needed_bytes < avail_bytes);

			if (!preserve_content)
				warning("not enough spare RAM to preserve content (",
				        "need ", Genode::Number_of_bytes(needed_bytes), ", "
				        "have ", Genode::Number_of_bytes(avail_bytes), ")");

			if (preserve_content && _text_screen_surface.constructed())
				snapshot.construct(_heap, *_text_screen_surface);

			Position const orig_cursor_pos = _text_screen_surface.constructed()
			                               ? _text_screen_surface->cursor_pos()
			                               : Position();

			_text_screen_surface.construct(_heap, _font->font(),
			                               _color_palette, _framebuffer);

			if (snapshot.constructed())
				_text_screen_surface->import(*snapshot);

			_text_screen_surface->cursor_pos(orig_cursor_pos);

			_terminal_size = _text_screen_surface->size();

		} else {
			_text_screen_surface->geometry(new_geometry);
		}

		_framebuffer.switch_to_new_view(new_geometry.used_pixels());
	}
	catch (Text_screen_surface<PT>::Geometry::Invalid) {

		/*
		 * Make sure to never operate on an invalid-sized framebuffer
		 *
		 * If the exception is thrown by the construction of 'new_geometry',
		 * there may still be a stale '_text_screen_surface'.
		 */
		_text_screen_surface.destruct();
		_terminal_size = Area(0, 0);

		_framebuffer.switch_to_new_view(_terminal_size);
	}

	_root.notify_resized(Session::Size(_terminal_size.w(), _terminal_size.h()));
	_schedule_flush();
}

void Terminal::Main::_handle_config()
{
	_config.update();

	Xml_node const config = _config.xml();

	_color_palette.apply_config(config);

	_font.destruct();

	_root_dir.apply_config(config.sub_node("vfs"));

	Cached_font::Limit const cache_limit {
		config.attribute_value("cache", Number_of_bytes(256*1024)) };

	_font.construct(_heap, _root_dir, cache_limit);

	Area char_box = _font->font().bounding_box();

	unsigned config_width  = config.attribute_value("initial_columns", 80U);
	unsigned config_height = config.attribute_value("initial_rows", 24U);

	Area area(config_width * char_box.w(),
	          config_height * char_box.h());

	_framebuffer.switch_to_new_mode(area);
	_adjust_geometry();
}


void Terminal::Main::_handle_mode()
{
	::Framebuffer::Mode new_mode = _nitpicker.mode();

	Area char_box = _font->font().bounding_box();

	unsigned w = new_mode.width();
	unsigned h = new_mode.height();
	if (!(w&h)) {
		_root.notify_resized(Session::Size(0,0));
		return;
	}

	w -= w % char_box.w();
	h -= h % char_box.h();

	_framebuffer.switch_to_new_mode(Area(w,h));
	_adjust_geometry();
}


void Terminal::Main::_handle_input()
{
	_nitpicker.input()->for_each_event([&] (Input::Event const &event) {

		event.handle_press([&] (Input::Keycode, Codepoint codepoint) {

			/* function-key unicodes */
			enum {
				CODEPOINT_UP     = 0xf700, CODEPOINT_DOWN     = 0xf701,
				CODEPOINT_LEFT   = 0xf702, CODEPOINT_RIGHT    = 0xf703,
				CODEPOINT_F1     = 0xf704, CODEPOINT_F2       = 0xf705,
				CODEPOINT_F3     = 0xf706, CODEPOINT_F4       = 0xf707,
				CODEPOINT_F5     = 0xf708, CODEPOINT_F6       = 0xf709,
				CODEPOINT_F7     = 0xf70a, CODEPOINT_F8       = 0xf70b,
				CODEPOINT_F9     = 0xf70c, CODEPOINT_F10      = 0xf70d,
				CODEPOINT_F11    = 0xf70e, CODEPOINT_F12      = 0xf70f,
				CODEPOINT_HOME   = 0xf729, CODEPOINT_INSERT   = 0xf727,
				CODEPOINT_DELETE = 0xf728, CODEPOINT_END      = 0xf72b,
				CODEPOINT_PAGEUP = 0xf72c, CODEPOINT_PAGEDOWN = 0xf72d,
			};

			char const *special_sequence = nullptr;
			switch (codepoint.value) {
			case CODEPOINT_UP:       special_sequence = "\EOA";   break;
			case CODEPOINT_DOWN:     special_sequence = "\EOB";   break;
			case CODEPOINT_LEFT:     special_sequence = "\EOD";   break;
			case CODEPOINT_RIGHT:    special_sequence = "\EOC";   break;
			case CODEPOINT_F1:       special_sequence = "\EOP";   break;
			case CODEPOINT_F2:       special_sequence = "\EOQ";   break;
			case CODEPOINT_F3:       special_sequence = "\EOR";   break;
			case CODEPOINT_F4:       special_sequence = "\EOS";   break;
			case CODEPOINT_F5:       special_sequence = "\E[15~"; break;
			case CODEPOINT_F6:       special_sequence = "\E[17~"; break;
			case CODEPOINT_F7:       special_sequence = "\E[18~"; break;
			case CODEPOINT_F8:       special_sequence = "\E[19~"; break;
			case CODEPOINT_F9:       special_sequence = "\E[20~"; break;
			case CODEPOINT_F10:      special_sequence = "\E[21~"; break;
			case CODEPOINT_F11:      special_sequence = "\E[23~"; break;
			case CODEPOINT_F12:      special_sequence = "\E[24~"; break;
			case CODEPOINT_HOME:     special_sequence = "\E[1~";  break;
			case CODEPOINT_INSERT:   special_sequence = "\E[2~";  break;
			case CODEPOINT_DELETE:   special_sequence = "\E[3~";  break;
			case CODEPOINT_END:      special_sequence = "\E[4~";  break;
			case CODEPOINT_PAGEUP:   special_sequence = "\E[5~";  break;
			case CODEPOINT_PAGEDOWN: special_sequence = "\E[6~";  break;
			};

			if (special_sequence)
				_read_buffer.add(special_sequence);
			else
				_read_buffer.add(codepoint);
		});
	});
}


void Component::construct(Genode::Env &env) { static Terminal::Main main(env); }
