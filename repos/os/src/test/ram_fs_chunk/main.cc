/*
 * \brief  Unit test for RAM fs chunk data structure
 * \author Norman Feske
 * \date   2012-04-19
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <ram_fs/chunk.h>

namespace Ram_fs {
	typedef Chunk<2>                      Chunk_level_3;
	typedef Chunk_index<3, Chunk_level_3> Chunk_level_2;
	typedef Chunk_index<4, Chunk_level_2> Chunk_level_1;
	typedef Chunk_index<5, Chunk_level_1> Chunk_level_0;
}


namespace Genode {

	struct Allocator_tracer : Allocator
	{
		size_t     _sum;
		Allocator &_wrapped;

		Allocator_tracer(Allocator &wrapped) : _sum(0), _wrapped(wrapped) { }

		size_t sum() const { return _sum; }

		bool alloc(size_t size, void **out_addr)
		{
			_sum += size;
			return _wrapped.alloc(size, out_addr);
		}

		void free(void *addr, size_t size)
		{
			_sum -= size;
			_wrapped.free(addr, size);
		}

		size_t overhead(size_t size) const override { return _wrapped.overhead(size); }
		bool    need_size_for_free() const override { return _wrapped.need_size_for_free(); }
	};

	/**
	 * Helper for the formatted output of a chunk state
	 */
	static inline void print(Output &out, Ram_fs::Chunk_level_0 const &chunk)
	{
		using namespace Ram_fs;

		size_t used_size = chunk.used_size();

		struct File_size_out_of_bounds { };
		if (used_size > Chunk_level_0::SIZE)
			throw File_size_out_of_bounds();

		Genode::print(out, "content (size=", used_size, "): ");

		auto print_fn = [&] (char const *src, size_t src_len) {
			if (src)
				for (unsigned i = 0; i < src_len; ++i) {
					char const c = src[i];
					if (c)
						Genode::print(out, Genode::Char(c));
					else
						Genode::print(out, ".");
				}
			else
				for (unsigned i = 0; i < src_len; ++i)
					Genode::print(out, ".");
		};

		Genode::print(out, "\"");
		chunk.read(print_fn, used_size, 0);
		Genode::print(out, "\"");
	}

};


static void write(Ram_fs::Chunk_level_0 &chunk,
                  char const *str, Genode::off_t seek_offset)
{
	using namespace Genode;

	char const *src = str;
	size_t len = strlen(src);

	auto write_fn = [&] (char *dst, size_t dst_len) {
		size_t n = min(len, dst_len);
		memcpy(dst, src, n);
		len -= n;
		src += n;
	};

	chunk.write(write_fn, len, seek_offset);
	log("write \"", str, "\" at offset ", seek_offset, " -> ", chunk);
}


static void truncate(Ram_fs::Chunk_level_0 &chunk,
                     File_system::file_size_t size)
{
	chunk.truncate(size);
	Genode::log("trunc(", size, ") -> ", chunk);
}


int main(int, char **)
{
	using namespace File_system;
	using namespace Ram_fs;
	using namespace Genode;

	log("--- ram_fs_chunk test ---");

	log("chunk sizes");
	log("  level 0: payload=", (int)Chunk_level_0::SIZE, " sizeof=", sizeof(Chunk_level_0));
	log("  level 1: payload=", (int)Chunk_level_1::SIZE, " sizeof=", sizeof(Chunk_level_1));
	log("  level 2: payload=", (int)Chunk_level_2::SIZE, " sizeof=", sizeof(Chunk_level_2));
	log("  level 3: payload=", (int)Chunk_level_3::SIZE, " sizeof=", sizeof(Chunk_level_3));

	static Allocator_tracer alloc(*env()->heap());

	{
		Chunk_level_0 chunk(alloc, 0);

		write(chunk, "five-o-one", 0);

		/* overwrite part of the file */
		write(chunk, "five", 7);

		/* write to position beyond current file length */
		write(chunk, "Nuance", 17);
		write(chunk, "YM-2149", 35);

		truncate(chunk, 30);

		for (unsigned i = 29; i > 0; i--)
			truncate(chunk, i);
	}

	log("allocator: sum=", alloc.sum());

	return 0;
}
