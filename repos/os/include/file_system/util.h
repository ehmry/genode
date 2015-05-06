/*
 * \brief  Utilities
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _FILE_SYSTEM__UTIL_H_
#define _FILE_SYSTEM__UTIL_H_

namespace File_system {

	/**
	 * Return base-name portion of null-terminated path string
	 */
	static inline char const *basename(char const *path)
	{
		char const *start = path;

		for (; *path; path++)
			if (*path == '/')
				start = path + 1;

		return start;
	}


	/**
	 * Return true if specified path is a base name (contains no path delimiters)
	 */
	static inline bool is_basename(char const *path)
	{
		for (; *path; path++)
			if (*path == '/')
				return false;

		return true;
	}


	/**
	 * Return true if character 'c' occurs in null-terminated string 'str'
 	*/
	static inline bool string_contains(char const *str, char c)
	{
		for (; *str; str++)
			if (*str == c)
				return true;
		return false;
	}


	/**
	 * Return true if 'str' is a valid node name
	 */
	static inline bool valid_name(char const *str)
	{
		if (string_contains(str, '/')) return false;

		/* must have at least one character */
		if (str[0] == 0) return false;

		return true;
	}


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


	struct Handle_guard
	{
		private:

			Session     &_session;
			Node_handle  _handle;

		public:

			Handle_guard(Session &session, Node_handle handle)
			: _session(session), _handle(handle) { }

			~Handle_guard() { _session.close(_handle); }
	};
}

#endif /* _FILE_SYSTEM__UTIL_H_ */
