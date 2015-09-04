		/**
		 * Copy the content of a file to another and delete.
		 *
		 * XXX: should the packet stream be flushed first?
		 */
		void _move(char const *from_path, char const *to_path)
		{
			Directory_service::Stat from_stat;
			Directory_service::Stat   to_stat;
			Vfs_handle *from_handle = nullptr;
			Vfs_handle   *to_handle = nullptr;

			assert_stat(root()->stat(from_path, from_stat));
			assert_stat(root()->stat(  to_path,   to_stat));

			assert_open(root()->open(
				from_path, Directory_service::OPEN_MODE_RDONLY, &from_handle));
			Vfs_handle::Guard from_guard(from_handle);
			assert_open(root()->open(
				 to_path, Directory_service::OPEN_MODE_WRONLY,    &to_handle));
			Vfs_handle::Guard to_guard(to_handle);

			/* make sure there is enough space at the destination */
			assert_truncate(to_handle->fs().ftruncate(to_handle, from_stat.size));

			/* allocate a packet to use as buffer */
			size_t const buf_size = tx()->bulk_buffer_size() / 2;
			Packet_descriptor dummy_packet = tx()->alloc_packet(buf_size);
			char *buf = tx()->packet_content(dummy_packet);

			Vfs::file_size copied = 0;
			while (copied < from_stat.size) {
				Vfs::file_size n = 0;

				from_handle->fs().read(from_handle, buf, buf_size, n);
				  to_handle->fs().write( to_handle, buf, n, n);

				if (n == 0) {
					PERR("move failed");
					tx()->release_packet(dummy_packet);
					throw Permission_denied();
				}
				copied += n;
			}
			tx()->release_packet(dummy_packet);
			assert_unlink(root()->unlink(from_path));
		}