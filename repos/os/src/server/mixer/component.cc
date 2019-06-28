/*
 * \brief  Audio mixer component
 * \author Emery Hemingway
 * \date   2019-06-28
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio_in_session/audio_in_session.h>
#include <audio_in_session/capability.h>
#include <audio_out_session/audio_out_session.h>
#include <audio_out_session/capability.h>
#include <audio/buffer.h>
#include <os/session_policy.h>
#include <base/registry.h>
#include <base/attached_ram_dataspace.h>
#include <base/rpc_server.h>
#include <base/heap.h>
#include <base/component.h>

/* local includes */
#include "session_requests.h"

namespace Mixer
{
	using namespace Genode;

	struct Output_component;
	struct  Input_component;
	struct Main;


	typedef Genode::Registered<Input_component> Registered_input;
	typedef Genode::Registry<Registered_input>  Registered_inputs;
}


struct Mixer::Output_component
	: public Genode::Rpc_object<Audio_in::Session, Output_component>
{
		Audio::Stream_buffer       buffer;
		Signal_context_capability  progress_cap;
		Session_label const        label;

		Parent::Server::Id const session_id;

		Output_component(Parent::Server::Id id,
		                 Session_label &label,
		                 Genode::Ram_allocator &ram, Genode::Region_map &rm,
		                 Signal_context_capability progress)
		: buffer(ram, rm), progress_cap(progress), label(label), session_id(id)
		{ }

		/************************
		 ** Audio_in interface **
		 ************************/

		Genode::Dataspace_capability dataspace() {
			return buffer.cap(); }

		void start_sigh(Signal_context_capability) override { }

		void reset_sigh(Signal_context_capability) override { }

		Signal_context_capability progress_sigh() override {
			return progress_cap; }
};


struct Mixer::Input_component
	: public Genode::Rpc_object<Audio_out::Session, Input_component>
{
		Audio::Stream_buffer       buffer;
		Signal_context_capability  progress_cap { };
		Session_label const        label;
		Parent::Server::Id const   session_id;
		unsigned const             channel;

		Input_component (Parent::Server::Id id,
		                 Session_label &label, unsigned channel,
		                 Genode::Ram_allocator &ram, Genode::Region_map &rm)
		: buffer(ram, rm), label(label), session_id(id), channel(channel)
		{ }


		/************************
		 ** Audio_out interface **
		 ************************/

		Genode::Dataspace_capability dataspace() {
			return buffer.cap(); }

		void start() override { }

		void progress_sigh(Signal_context_capability sigh) override
		{
			progress_cap = sigh;
			if (progress_cap.valid())
				Signal_transmitter(progress_cap).submit();
		}
};


struct Mixer::Main : Genode::Session_request_handler
{
	enum { CHANNELS = 1<<1 };

	Genode::Env &_env;
	Sliced_heap  _sliced_heap { _env.pd(), _env.rm() };

	Attached_rom_dataspace _config_rom       { _env, "config" };
	Session_requests_rom   _requests_handler { _env, *this };

	Signal_handler<Main> _progress_handler
		{ _env.ep(), *this, &Main::_progress };

	Constructible<Output_component> _output_0 { };
	Constructible<Output_component> _output_1 { };

	Constructible<Output_component> *_outputs[CHANNELS] { &_output_0, &_output_1 };

	Registered_inputs _inputs { };

	void _progress()
	{
		using namespace Audio;

		for (int i = 0; i < CHANNELS; ++i)
			if (!_outputs[i]->constructed())
				return;

		unsigned pos = (*_outputs[0])->buffer.stream_sink().record_pos();

		Packet *out_pkts[CHANNELS];
		for (int i = 0; i < CHANNELS; ++i) {
			out_pkts[i] = (*_outputs[i])->buffer.stream_sink().get(pos);
			out_pkts[i]->content(nullptr, 0);
		}

		_inputs.for_each([&] (Registered_input &input) {
			input.buffer.stream_source().play(pos, [&] (Packet &in) {
				Packet &out = *out_pkts[input.channel];
				for (unsigned sample = 0; sample < Audio::PERIOD; ++sample) {
					out.content()[sample] += in.content()[sample];
				}
			});
		});

		for (int i = 0; i < CHANNELS; ++i) {
			(*_outputs[i])->buffer.stream_sink().increment_position();
		}
	}

	Main(Genode::Env &env) : _env(env) { }

	void handle_session_create(Session_state::Name const &name,
	                           Parent::Server::Id id,
	                           Session_state::Args const &args) override
	{
		_config_rom.update();

		Session_label label = label_from_args(args.string());
		Session_policy policy(label, _config_rom.xml());

		// only two channels supported, so drop all but the first channel bit
		unsigned const chan = (CHANNELS-1) & policy.attribute_value("channel", 0U);

		if (name == "Audio_out") {
			Input_component *session = new (_sliced_heap)
				Registered_input(_inputs, id, label, chan, _env.pd(), _env.rm());

			_env.parent().deliver_session_cap(id, _env.ep().manage(*session));

		} else

		if (name == "Audio_in") {
			Audio_in::Session_capability session_cap { };
			if (_outputs[chan]->constructed())
				return; // defer request

			// Allow channel 0 to drive progress handler
			_outputs[chan]->construct(
				id, label, _env.pd(), _env.rm(),
				chan == 0 ? _progress_handler : Signal_context_capability());

			_env.parent().deliver_session_cap(id, _env.ep().manage(*(*_outputs[chan])));

		} else
			throw Service_denied();
	}

	void handle_session_close(Parent::Server::Id id) override
	{
		Input_component *input { nullptr };
		_inputs.for_each([&] (Registered_input &other) {
			if (!input && other.session_id == id)
				input = &other;
		});

		if (input)
			destroy(_sliced_heap, input);
		else {
			for (int i = 0; i < CHANNELS; ++i) {
				if ((*_outputs[i])->session_id == id) {
					(*_outputs[i]).destruct();
				}
			}
		}
	}
};



void Component::construct(Genode::Env &env)
{
	static Mixer::Main inst(env);
}
