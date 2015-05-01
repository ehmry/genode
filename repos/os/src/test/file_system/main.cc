#include <hash/blake2s.h>
#include <file_system_session/connection.h>
#include <base/allocator_avl.h>
#include <os/config.h>
#include <timer_session/connection.h>

/*
 * XXX The following generic utilities should be moved to a public place.
 *     They are based on those found in the 'libc_fs' plugin. We should
 *     unify them.
 */

namespace File_system {
	/**
	 * Collect pending packet acknowledgements, freeing the space occupied
	 * by the packet in the bulk buffer
	 *
	 * This function should be called prior enqueing new packets into the
	 * packet stream to free up space in the bulk buffer.
	 */
	static void collect_acknowledgements(Session::Tx::Source &source)
	{
		while (source.ack_avail())
			source.release_packet(source.get_acked_packet());
	}

	/**
	 * Write file content
	 */
	static inline size_t write(Session &fs, File_handle const &file_handle,
	                          void const *src, size_t count, off_t seek_offset = 0)
	{
		Session::Tx::Source &source = *fs.tx();

		size_t const max_packet_size = source.bulk_buffer_size() / 2;

		size_t remaining_count = count;

		while (remaining_count) {

			collect_acknowledgements(source);

			size_t const curr_packet_size = min(remaining_count, max_packet_size);

			Packet_descriptor
				packet(source.alloc_packet(curr_packet_size),
				       0,
				       file_handle,
				       File_system::Packet_descriptor::WRITE,
				       curr_packet_size,
				       seek_offset);

			/* copy-out source buffer into payload */
			memcpy(source.packet_content(packet), src, curr_packet_size);

			/* pass packet to server side */
			source.submit_packet(packet);

			packet = source.get_acked_packet();;

			source.release_packet(packet);

			/* prepare next iteration */
			seek_offset += curr_packet_size;
			src = (void *)((Genode::addr_t)src + curr_packet_size);
			remaining_count -= curr_packet_size;
		}

		return count - remaining_count;
	}

	/**
	 * Read file content
	 */
	static inline size_t read(Session &fs, File_handle const &file_handle,
	                          void *dst, size_t count, off_t seek_offset = 0)
	{
		Session::Tx::Source &source = *fs.tx();

		size_t const max_packet_size = source.bulk_buffer_size() / 2;

		size_t remaining_count = count;

		while (remaining_count) {

			collect_acknowledgements(source);

			size_t const curr_packet_size = min(remaining_count, max_packet_size);

			Packet_descriptor
				packet(source.alloc_packet(curr_packet_size),
				       0,
				       file_handle,
				       File_system::Packet_descriptor::READ,
				       curr_packet_size,
				       seek_offset);

			/* pass packet to server side */
			source.submit_packet(packet);

			packet = source.get_acked_packet();

			size_t const read_num_bytes = min(packet.length(), curr_packet_size);

			/* copy-out payload into destination buffer */
			memcpy(dst, source.packet_content(packet), read_num_bytes);

			source.release_packet(packet);

			/* prepare next iteration */
			seek_offset += read_num_bytes;
			dst = (void *)((Genode::addr_t)dst + read_num_bytes);
			remaining_count -= read_num_bytes;

			/*
			 * If we received less bytes than requested, we reached the end
			 * of the file.
			 */
			if (read_num_bytes < curr_packet_size)
				break;
		}

		return count - remaining_count;
	}
}

/**
 * Randomize the message.
 */
void trash_buffer(Hash::Function *hash, size_t chunk, uint8_t *buf, size_t len)
{
 	hash->digest(buf, chunk);
	for (size_t i = 0; i < len;) {
		hash->update(&buf[i], chunk);
		i += chunk;
		hash->digest(&buf[i], chunk);
	}
}

int main()
{
	using namespace File_system;

	Genode::Allocator_avl   tx_block_alloc(Genode::env()->heap());
	File_system::Connection fs(tx_block_alloc);
	Timer::Connection       timer;
	Hash::Blake2s           hash;

	size_t chunk = hash.size();
	uint8_t msg[chunk*64];
	uint8_t md1[chunk];
	uint8_t md2[chunk];

	Dir_handle  root = fs.dir("/", false);
	File_handle file = fs.file(root, "testfile1", READ_WRITE, true);
	fs.close(root);

	unsigned int iterations = 1;
	try {
		Genode::config()->xml_node().attribute("iterations").value(&iterations);
	} catch(...) { }

	size_t count = 0;
	unsigned long then = timer.elapsed_ms();
	for (unsigned int i = 0; i < iterations; ++i) {
		trash_buffer(&hash, chunk, msg, sizeof(msg));
		hash.reset();
		hash.update(msg, sizeof(msg));
		hash.digest(md1, sizeof(md1));

		count += write(fs, file, msg, sizeof(msg));
		memset(msg, 0x00, sizeof(msg));
		count += read( fs, file, msg, sizeof(msg));

		hash.reset();
		hash.update(msg, sizeof(msg));
		hash.digest(md2, sizeof(md2));

		if (memcmp(md1, md2, sizeof(md1))) {
			PINF("---  File_system test failed on iteration #%d  ---", i);
			fs.close(file);
			return -1;
		}
	}

	fs.close(file);
	unsigned long elapsed = timer.elapsed_ms() - then;
	PINF("--- hash/write/read/hash %lu bytes in %lu.%lu seconds ---",
		 count, elapsed/1000, elapsed%1000);
	PINF("--- File_system test finished ---");
}