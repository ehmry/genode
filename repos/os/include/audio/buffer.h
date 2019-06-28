/*
 * \brief  Server-side audio packet packet buffer
 * \author Emery Hemingway
 * \date   2018-06-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO__BUFFER_H_
#define _INCLUDE__AUDIO__BUFFER_H_

#include <audio/stream.h>
#include <base/attached_ram_dataspace.h>

namespace Audio { struct Stream_buffer; }

struct Audio::Stream_buffer
{
	Genode::Attached_ram_dataspace _shared_ds;

	Stream_buffer(Genode::Ram_allocator &ram, Genode::Region_map &rm)
	: _shared_ds(ram, rm, sizeof(Audio::Stream)) { }

	Stream_sink &stream_sink() {
		return *_shared_ds.local_addr<Stream_sink>(); }

	Stream_source &stream_source() {
		return *_shared_ds.local_addr<Stream_source>(); }

	Genode::Dataspace_capability cap() {
		return _shared_ds.cap(); }
};

#endif /* _INCLUDE__AUDIO__BUFFER_H_ */