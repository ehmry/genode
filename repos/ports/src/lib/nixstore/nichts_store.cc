/*
 * \brief  Store-api to Nichts_store glue
 * \author Emery Hemingway
 * \date
 */

#include "nichts_store.h"

#include <base/lock.h>


bool
nix::willBuildLocally(const Derivation & drv) { NOT_IMP; return true; }

void
nix::canonicaliseTimestampAndPermissions(std::string const&) { NOT_IMP; }

using namespace nix;


/* Check whether a path is valid. */ 
bool
Nichts::Store_client::isValidPath(const nix::Path & path) {
			NOT_IMP; return false; };


/* Query which of the given paths is valid. */
PathSet
Nichts::Store_client::queryValidPaths(const PathSet & paths) {
			NOT_IMP; return PathSet(); };

		/* Query the set of all valid paths. */
		PathSet Nichts::Store_client::queryAllValidPaths() {
			NOT_IMP; return PathSet(); };

		/* Query information about a valid path. */
		ValidPathInfo Nichts::Store_client::queryPathInfo(const Path & path) {
			NOT_IMP; return ValidPathInfo(); };

		/* Query the hash of a valid path. */ 
		Hash Nichts::Store_client::queryPathHash(const Path & path) {
			NOT_IMP; return Hash(); };

		/* Query the set of outgoing FS references for a store path.	The
			 result is not cleared. */
		void Nichts::Store_client::queryReferences(const Path & path,
				PathSet & references) { NOT_IMP; };

		/* Queries the set of incoming FS references for a store path.
			 The result is not cleared. */
		void Nichts::Store_client::queryReferrers(const Path & path,
				PathSet & referrers) { NOT_IMP; };

		/* Query the deriver of a store path.	Return the empty string if
			 no deriver has been set. */
		Path Nichts::Store_client::queryDeriver(const Path & path) {
			NOT_IMP; return Path(); };

		/* Return all currently valid derivations that have `path' as an
			 output.	(Note that the result of `queryDeriver()' is the
			 derivation that was actually used to produce `path', which may
			 not exist anymore.) */
		PathSet Nichts::Store_client::queryValidDerivers(const Path & path) {
			NOT_IMP; return PathSet(); };
		/* Query the outputs of the derivation denoted by `path'. */
		PathSet Nichts::Store_client::queryDerivationOutputs(const Path & path) {
			NOT_IMP; return PathSet(); };

		/* Query the output names of the derivation denoted by `path'. */
		StringSet Nichts::Store_client::queryDerivationOutputNames(const Path & path) {
			NOT_IMP; return StringSet(); };

		/* Query the full store path given the hash part of a valid store
			 path, or "" if the path doesn't exist. */
		Path Nichts::Store_client::queryPathFromHashPart(const string & hashPart) {
			NOT_IMP; return Path(); };

		/* Query which of the given paths have substitutes. */
		PathSet Nichts::Store_client::querySubstitutablePaths(const PathSet & paths) {
			NOT_IMP; return PathSet(); };

		/* Query substitute info (i.e. references, derivers and download
			 sizes) of a set of paths.	If a path does not have substitute
			 info, it's omitted from the resulting ‘infos’ map. */
		void Nichts::Store_client::querySubstitutablePathInfos(const PathSet & paths,
				SubstitutablePathInfos & infos) { NOT_IMP; };

		/**
		 * Copy the contents of a path to the store and register the
		 * validity the resulting path.	The resulting path is returned.
		 * The function object `filter' can be used to exclude files (see
		 * libutil/archive.hh).
		 */
		Path Nichts::Store_client::addToStore(const Path & srcPath,
				bool recursive, HashType hashAlgo,
				PathFilter & filter, bool repair)
		{
			/* Protect from packets getting out of order. */
			static Genode::Lock lock;
			Genode::Lock_guard<Genode::Lock> guard(lock);

			using namespace File_system;

			char const *src_path = srcPath.c_str();
			// TODO: fix this
			char const *bname = src_path;
			for (int i = 0; src_path[i]; ++i)
				if (src_path[i] == '/')
					bname = src_path +i+1;

			char buf[1024];
			strncpy(buf, src_path, bname - src_path);
			PLOG("basename - %s, dirname - %s", bname, buf);

			/*****************
			 ** Source file **
			 *****************/
			Dir_handle dir = _fs.dir(File_system::Path(src_path, bname - src_path), false);
			File_handle src_handle = _fs.file(dir, bname,
			                                  READ_ONLY, false);
			_fs.close(dir);
			File_system::Status st = _fs.status(src_handle);

			/* Keep this a multiple of 512 for the sake of SHA256 server-side. */
			enum { BLOCK_SIZE = 4096 };
			size_t block_size = max(st.size, BLOCK_SIZE);

			/*************
			 ** Packets **
			 *************/
			File_system::Session::Tx::Source &src_source = *_fs.tx();
			File_system::Packet_descriptor src_packet = src_source.alloc_packet(block_size);
			File_system::Session::Tx::Source &dst_source = *_store.tx();
			File_system::Packet_descriptor dst_packet = dst_source.alloc_packet(block_size);

			/********************
			 ** Store desposit **
			 ********************/
			// TODO: get the root of the store
			Dir_handle store_dir = _store.dir("/", false);
			File_handle dst_handle = _store.file(store_dir, bname,
			                                     File_system::WRITE_ONLY, true);

			/**************
			 ** Exchange **
			 **************/
			for (size_t offset = 0; offset < st.size; offset += src_packet.length()) {

				File_system::Packet_descriptor sp(src_packet, 0, src_handle,
				                                  File_system::Packet_descriptor::READ,
				                                  block_size, offset);
				File_system::Packet_descriptor dp(dst_packet, 0, dst_handle,
				                                  File_system::Packet_descriptor::WRITE,
				                                  block_size, offset);

				src_source.submit_packet(sp);
				sp = src_source.get_acked_packet();
				if (!sp.succeeded()) throw nix::Error("addPathToStore: reading failed");
				memcpy(dst_source.packet_content(dp), src_source.packet_content(sp), sp.length());
				dst_source.submit_packet(dp);
				dp = dst_source.get_acked_packet();
				if (!sp.succeeded()) throw nix::Error("addPathToStore: writing failed");
			}

			src_source.release_packet(src_packet);
			dst_source.release_packet(dst_packet);

			_fs.close(src_handle);
			Nichts_store::Store_path store_path = _store.hash(dst_handle);
			_store.close(dst_handle);

			return nix::Path(store_path.string());
		}

		/**
		 * Like addToStore, but the contents written to the output path is
		 * a regular file containing the given string.
		 */
		Path Nichts::Store_client::addTextToStore(const string & name, const string & s,
				const PathSet & references, bool repair) {
			NOT_IMP; return Path(); };

		/* Export a store path, that is, create a NAR dump of the store
			 path and append its references and its deriver.	Optionally, a
			 cryptographic signature (created by OpenSSL) of the preceding
			 data is attached. */
		void Nichts::Store_client::exportPath(const Path & path, bool sign,
				Sink & sink) { NOT_IMP; };

		/* Import a sequence of NAR dumps created by exportPaths() into
			 the Nix store. */
		Paths Nichts::Store_client::importPaths(bool requireSignature, Source & source) {
			NOT_IMP; return Paths(); };

		/* For each path, if it's a derivation, build it.	Building a
			 derivation means ensuring that the output paths are valid.	If
			 they are already valid, this is a no-op.	Otherwise, validity
			 can be reached in two ways.	First, if the output paths is
			 substitutable, then build the path that way.	Second, the
			 output paths can be created by running the builder, after
			 recursively building any sub-derivations. For inputs that are
			 not derivations, substitute them. */
		void Nichts::Store_client::buildPaths(const PathSet & paths, BuildMode buildMode) { NOT_IMP; };

		/* Ensure that a path is valid.	If it is not currently valid, it
			 may be made valid by running a substitute (if defined for the
			 path). */
		void Nichts::Store_client::ensurePath(const Path & path) { NOT_IMP; };

		/* Add a store path as a temporary root of the garbage collector.
			 The root disappears as soon as we exit. */
		void Nichts::Store_client::addTempRoot(const Path & path) { NOT_IMP; };

		/* Add an indirect root, which is merely a symlink to `path' from
			 /nix/var/nix/gcroots/auto/<hash of `path'>.	`path' is supposed
			 to be a symlink to a store path.	The garbage collector will
			 automatically remove the indirect root when it finds that
			 `path' has disappeared. */
		void Nichts::Store_client::addIndirectRoot(const Path & path) { NOT_IMP; };

		/* Acquire the global GC lock, then immediately release it.	This
			 function must be called after registering a new permanent root,
			 but before exiting.	Otherwise, it is possible that a running
			 garbage collector doesn't see the new root and deletes the
			 stuff we've just built.	By acquiring the lock briefly, we
			 ensure that either:

			 - The collector is already running, and so we block until the
				 collector is finished.	The collector will know about our
				 *temporary* locks, which should include whatever it is we
				 want to register as a permanent lock.

			 - The collector isn't running, or it's just started but hasn't
				 acquired the GC lock yet.	In that case we get and release
				 the lock right away, then exit.	The collector scans the
				 permanent root and sees our's.

			 In either case the permanent root is seen by the collector. */
		void Nichts::Store_client::syncWithGC() { NOT_IMP; };

		/* Find the roots of the garbage collector.	Each root is a pair
			 (link, storepath) where `link' is the path of the symlink
			 outside of the Nix store that point to `storePath'.	*/
		Roots Nichts::Store_client::findRoots() { NOT_IMP; return Roots(); };

		/* Perform a garbage collection. */
		void Nichts::Store_client::collectGarbage(const GCOptions & options, GCResults & results) { NOT_IMP; };

		/* Return the set of paths that have failed to build.*/
		PathSet Nichts::Store_client::queryFailedPaths() {
			NOT_IMP; return PathSet(); };

		/* Clear the "failed" status of the given paths.	The special
			 value `*' causes all failed paths to be cleared. */
		void Nichts::Store_client::clearFailedPaths(const PathSet & paths) { NOT_IMP; };

		/* Return a string representing information about the path that
			 can be loaded into the database using `nix-store --load-db' or
			 `nix-store --register-validity'. */
		string Nichts::Store_client::makeValidityRegistration(const PathSet & paths,
				bool showDerivers, bool showHash) { NOT_IMP; return string(); }

		/* Optimise the disk space usage of the Nix store by hard-linking files
			 with the same contents. */
		void Nichts::Store_client::optimiseStore() { NOT_IMP; };
