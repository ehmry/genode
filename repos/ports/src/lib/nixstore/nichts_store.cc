/*
 * \brief  Store-api to Nichts_store glue
 * \author Emery Hemingway
 * \date
 */

/* Upstream Nix includes. */
#include "nichts_store.h"
#include <libutil/util.hh>

/* Libc includes. */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Genode includes. */
#include <base/printf.h>

#define NOT_IMP PERR("%s not implemented", __func__)

/* Keep this a multiple of 512 for the sake of SHA256 server-side. */
enum { BLOCK_SIZE = 4096 };

bool
nix::willBuildLocally(const Derivation & drv) { NOT_IMP; return true; }

void
nix::canonicaliseTimestampAndPermissions(std::string const&) { NOT_IMP; }


using namespace nix;
using namespace Nichts;


Store_client::Store_client()
:
	_tx_alloc(Genode::env()->heap()),
	_fs      (_tx_alloc),
	_store   (_tx_alloc)
{
	/* Open a handle to the root of the store file system. */
	_store_dir = _store.file_system()->dir("/nix/store", false);
};


Store_client::~Store_client()
{
	_store.file_system()->close(_store_dir);
};


Nichts_store::Path
Store_client::add_file(nix::Path const  &src_path)
{
	using namespace File_system;

	/*
	 * Access the file using libc to ensure
	 * the VFS is used for path resolution.
	 */
    AutoCloseFD fd = open(src_path.c_str(), O_RDONLY);
	if (fd == -1)
		throw SysError(format("opening file ‘%1%’") % src_path);

	struct stat st;
	if (fstat(fd, &st))
		throw SysError(format("getting size of file ‘%1%’") % src_path);
	size_t size = st.st_size;

	nix::Path name = src_path.substr(src_path.rfind("/")+1, src_path.size()-1);
	File_handle dst_handle = _store.file_system()->file(_store_dir, name.c_str(),
	                                                    WRITE_ONLY, true);
	_store.file_system()->truncate(dst_handle, size);

	size_t block_size = min(size, BLOCK_SIZE);
	File_system::Session::Tx::Source &source = *_store.file_system()->tx();
	File_system::Packet_descriptor packet = source.alloc_packet(block_size);

	/* Prevent packets getting out of order. */
	Genode::Lock::Guard guard(_packet_lock);
	for (size_t offset = 0; size;) {
		File_system::Packet_descriptor pck(
			packet, 0, dst_handle,
			File_system::Packet_descriptor::WRITE,
			block_size, offset);

		size_t n = ::read(fd, source.packet_content(pck), block_size);
		if (n != block_size)
			throw SysError(format("reading file ‘%1%’") % src_path);

		source.submit_packet(pck);
		pck = source.get_acked_packet();
		if (!pck.succeeded() || pck.length() != n)
				throw nix::Error(format("addPathToStore: writing `%1%' failed") % src_path);

		size -= block_size;
		offset += block_size;
		block_size = min(size, BLOCK_SIZE);
	}
	source.release_packet(packet);

	Nichts_store::Path store_path = _store.hash(dst_handle);
	_store.file_system()->close(dst_handle);

	return store_path;
}


/************************
 ** StoreAPI interface **
************************/

/* Check whether a path is valid. */ 
bool
Nichts::Store_client::isValidPath(const nix::Path & path) {
			NOT_IMP; return false; };


/* Query which of the given paths is valid. */
PathSet
Store_client::queryValidPaths(const PathSet & paths) {
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
Path
Store_client::addToStore(const Path & srcPath,
                         bool recursive, HashType hashAlgo,
                         PathFilter & filter, bool repair)
{
	PDBG("%s", srcPath.c_str());
	using namespace File_system;
	Nichts_store::Path out_path;

	Node_handle node = _fs.node(srcPath.c_str());
	Status src_st = _fs.status(node);
	_fs.close(node);
	if (src_st.mode == Status::MODE_FILE) {
		out_path = add_file(srcPath);
	} else
		throw SysError(format("adding non-regular files not implemented, not adding %1%") % srcPath);

	return nix::Path(out_path.string());
}

/**
 * Like addToStore, but the contents written to the output path is
 * a regular file containing the given string.
 */
Path Nichts::Store_client::addTextToStore(const string & name, const string & text,
	                                      const PathSet & references, bool repair)
{
	/* TODO: references? repair? */

	using namespace File_system;

	/* Prevent packets getting out of order. */
	Genode::Lock_guard<Genode::Lock> guard(_packet_lock);

	std::string::size_type n = text.size();

	File_handle dst_handle = _store.file_system()->file(
		_store_dir, name.c_str(), WRITE_ONLY, true);

	size_t block_size = min(n, BLOCK_SIZE);

	/* Packets */
	File_system::Session::Tx::Source &dst_source = *_store.file_system()->tx();
	File_system::Packet_descriptor dst_packet = dst_source.alloc_packet(BLOCK_SIZE);

	/* Exchange */
	for (size_t offset = 0; offset < n; offset += dst_packet.length()) {
		File_system::Packet_descriptor dst_pck(
			dst_packet, 0, dst_handle,
			File_system::Packet_descriptor::WRITE,
			block_size, offset);

		text.copy(dst_source.packet_content(dst_pck), text.size());

		dst_source.submit_packet(dst_pck);
		dst_pck = dst_source.get_acked_packet();
		if (!dst_pck.succeeded())
				throw nix::Error("addPathToStore: writing failed");
	}

	dst_source.release_packet(dst_packet);
	Nichts_store::Path store_path = _store.hash(dst_handle);
	_store.file_system()->close(dst_handle);

	return nix::Path(store_path.string());
};

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

/**
 * Optimise the disk space usage of the Nix store by hard-linking files
 * with the same contents.
  */
void
Nichts::Store_client::optimiseStore() { NOT_IMP; };
