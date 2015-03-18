/* Stdcxx includes */
#include <string>

/* Genode includes */
#include <base/printf.h>

#include "store.hh"

#include "config.h"
#include "globals.hh"
#include "archive.hh"
#include "pathlocks.hh"
#include "worker-protocol.hh"
#include "derivations.hh"
#include "affinity.hh"

#include <iostream>
#include <algorithm>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <grp.h>

#if HAVE_UNSHARE && HAVE_STATVFS && HAVE_SYS_MOUNT_H
#include <sched.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#endif

#if HAVE_LINUX_FS_H
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <errno.h>
#endif

#include <sqlite3.h>


#define NOT_IMP PDBG("%s not implemted", __func__);

/**
 * Return base-name portion of null-terminated path string
 */
static char const *basename(char const *path)
{
	char const *start = path;

	for (; *path; path++)
		if (*path == '/')
			start = path + 1;

	return start;
}


namespace nix {

	/* Nix store and database schema version.  Version 1 (or 0) was Nix <=
	 * 0.7.  Version 2 was Nix 0.8 and 0.9.  Version 3 is Nix 0.10.
	 * Version 4 is Nix 0.11.  Version 5 is Nix 0.12-0.16.  Version 6 is
	 * Nix 1.0.  Version 7 is Nix 1.3.
	 */
	const int nixSchemaVersion = 7;


MakeError(SQLiteError, Error);
MakeError(SQLiteBusy, SQLiteError);


static void throwSQLiteError(sqlite3 * db, const format & f)
    __attribute__ ((noreturn));

static void throwSQLiteError(sqlite3 * db, const format & f)
{
    int err = sqlite3_errcode(db);
    if (err == SQLITE_BUSY || err == SQLITE_PROTOCOL) {
        if (err == SQLITE_PROTOCOL)
            printMsg(lvlError, "warning: SQLite database is busy (SQLITE_PROTOCOL)");
        else {
            static bool warned = false;
            if (!warned) {
                printMsg(lvlError, "warning: SQLite database is busy");
                warned = true;
            }
        }
        /* Sleep for a while since retrying the transaction right away
           is likely to fail again. */
#if HAVE_NANOSLEEP
        struct timespec t;
        t.tv_sec = 0;
        t.tv_nsec = (random() % 100) * 1000 * 1000; /* <= 0.1s */
        nanosleep(&t, 0);
#else
        sleep(1);
#endif
        throw SQLiteBusy(format("%1%: %2%") % f.str() % sqlite3_errmsg(db));
    }
    else
        throw SQLiteError(format("%1%: %2%") % f.str() % sqlite3_errmsg(db));
}


/* Convenience macros for retrying a SQLite transaction. */
#define retry_sqlite while (1) { try {
#define end_retry_sqlite break; } catch (SQLiteBusy & e) { } }


SQLite::~SQLite()
{
    try {
        if (db && sqlite3_close(db) != SQLITE_OK)
            throwSQLiteError(db, "closing database");
    } catch (...) {
        ignoreException();
    }
}


void SQLiteStmt::create(sqlite3 * db, const string & s)
{
    checkInterrupt();
    assert(!stmt);
    if (sqlite3_prepare_v2(db, s.c_str(), -1, &stmt, 0) != SQLITE_OK)
        throwSQLiteError(db, "creating statement");
    this->db = db;
}


void SQLiteStmt::reset()
{
    assert(stmt);
    /* Note: sqlite3_reset() returns the error code for the most
       recent call to sqlite3_step().  So ignore it. */
    sqlite3_reset(stmt);
    curArg = 1;
}


SQLiteStmt::~SQLiteStmt()
{
    try {
        if (stmt && sqlite3_finalize(stmt) != SQLITE_OK)
            throwSQLiteError(db, "finalizing statement");
    } catch (...) {
        ignoreException();
    }
}


void SQLiteStmt::bind(const string & value)
{
    if (sqlite3_bind_text(stmt, curArg++, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
        throwSQLiteError(db, "binding argument");
}


void SQLiteStmt::bind(int value)
{
    if (sqlite3_bind_int(stmt, curArg++, value) != SQLITE_OK)
        throwSQLiteError(db, "binding argument");
}


void SQLiteStmt::bind64(long long value)
{
    if (sqlite3_bind_int64(stmt, curArg++, value) != SQLITE_OK)
        throwSQLiteError(db, "binding argument");
}


void SQLiteStmt::bind()
{
    if (sqlite3_bind_null(stmt, curArg++) != SQLITE_OK)
        throwSQLiteError(db, "binding argument");
}


/* Helper class to ensure that prepared statements are reset when
   leaving the scope that uses them.  Unfinished prepared statements
   prevent transactions from being aborted, and can cause locks to be
   kept when they should be released. */
struct SQLiteStmtUse
{
    SQLiteStmt & stmt;
    SQLiteStmtUse(SQLiteStmt & stmt) : stmt(stmt)
    {
        stmt.reset();
    }
    ~SQLiteStmtUse()
    {
        try {
            stmt.reset();
        } catch (...) {
            ignoreException();
        }
    }
};


struct SQLiteTxn
{
    bool active;
    sqlite3 * db;

    SQLiteTxn(sqlite3 * db) : active(false) {
        this->db = db;
        if (sqlite3_exec(db, "begin;", 0, 0, 0) != SQLITE_OK)
            throwSQLiteError(db, "starting transaction");
        active = true;
    }

    void commit()
    {
        if (sqlite3_exec(db, "commit;", 0, 0, 0) != SQLITE_OK)
            throwSQLiteError(db, "committing transaction");
        active = false;
    }

    ~SQLiteTxn()
    {
        try {
            if (active && sqlite3_exec(db, "rollback;", 0, 0, 0) != SQLITE_OK)
                throwSQLiteError(db, "aborting transaction");
        } catch (...) {
            ignoreException();
        }
    }
};


/* Store paths have the following form:

   <store>/<h>-<name>

   where

   <store> = the location of the Nix store, usually /nix/store
   
   <name> = a human readable name for the path, typically obtained
     from the name attribute of the derivation, or the name of the
     source file from which the store path is created.  For derivation
     outputs other than the default "out" output, the string "-<id>"
     is suffixed to <name>.
     
   <h> = base-32 representation of the first 160 bits of a SHA-256
     hash of <s>; the hash part of the store name
     
   <s> = the string "<type>:sha256:<h2>:<store>:<name>";
     note that it includes the location of the store as well as the
     name to make sure that changes to either of those are reflected
     in the hash (e.g. you won't get /nix/store/<h>-name1 and
     /nix/store/<h>-name2 with equal hash parts).
     
   <type> = one of:
     "text:<r1>:<r2>:...<rN>"
       for plain text files written to the store using
       addTextToStore(); <r1> ... <rN> are the references of the
       path.
     "source"
       for paths copied to the store using addToStore() when recursive
       = true and hashAlgo = "sha256"
     "output:<id>"
       for either the outputs created by derivations, OR paths copied
       to the store using addToStore() with recursive != true or
       hashAlgo != "sha256" (in that case "source" is used; it's
       silly, but it's done that way for compatibility).  <id> is the
       name of the output (usually, "out").

   <h2> = base-16 representation of a SHA-256 hash of:
     if <type> = "text:...":
       the string written to the resulting store path
     if <type> = "source":
       the serialisation of the path from which this store path is
       copied, as returned by hashPath()
     if <type> = "output:out":
       for non-fixed derivation outputs:
         the derivation (see hashDerivationModulo() in
         primops.cc)
       for paths copied by addToStore() or produced by fixed-output
       derivations:
         the string "fixed:out:<rec><algo>:<hash>:", where
           <rec> = "r:" for recursive (path) hashes, or "" or flat
             (file) hashes
           <algo> = "md5", "sha1" or "sha256"
           <hash> = base-16 representation of the path or flat hash of
             the contents of the path (or expected contents of the
             path for fixed-output derivations)

   It would have been nicer to handle fixed-output derivations under
   "source", e.g. have something like "source:<rec><algo>", but we're
   stuck with this for now...

   The main reason for this way of computing names is to prevent name
   collisions (for security).  For instance, it shouldn't be feasible
   to come up with a derivation whose output path collides with the
   path for a copied source.  The former would have a <s> starting with
   "output:out:", while the latter would have a <2> starting with
   "source:".
*/


void Store::create_dir(const char *path)
{
	using namespace File_system;
	try {
		Dir_handle dir = _fs.dir(File_system::Path(path), true);
		_fs.close(dir);
	}
	catch (Node_already_exists) {};
}


bool Store::path_exists(std::string const &path) { return path_exists(path.c_str()); }
bool Store::path_exists(const char *path)
{
	using namespace File_system;
	try {
		_fs.close(_fs.node(path));
	}
	catch (Lookup_failed) {
		return false;
	}
	return true;
}


void close_handle(File_system::Node_handle &node) { NOT_IMP; }


std::string Store::read_file(std::string const &path) { return read_file(path.c_str()); }
std::string Store::read_file(const char *path)
{
	NOT_IMP;

	return std::string();
}


void Store::write_file(Path &path, std::string str) { NOT_IMP; }
void Store::write_file(const char *path, std::string str) { NOT_IMP; }


void Store::delete_path(std::string const &path) { return delete_path(path.c_str()); }
void Store::delete_path(const char *path)
{
	using namespace File_system;

	Dir_handle dir;
	try {
		dir = dir_of(path);
		_fs.unlink(dir, basename(path));
	}
	catch (...) {
		_fs.close(dir);
	}
	_fs.close(dir);
}


Store::Store(bool reserve_space, bool read_only)
	:
		/* TODO: get a heap that the clients can replenish. */
		_fs_block_alloc(Genode::env()->heap()),
		_fs(_fs_block_alloc),
		schemaPath("/nix/state/db/schema"),
		didSetSubstituterEnv(false)
{
	if (read_only) {
		openDB(false);
		return;
	}

	/* Create missing state directories if they don't already exist. */
	_fs.close(dir_of("/nix/store/.links", true));
	_fs.close(dir_of("/nix/state/gcroots", true));
	_fs.close(dir_of("/nix/state/temproots", true));

	/*
	 * We can't open a SQLite database if the disk is full.  Since
	 * this prevents the garbage collector from running when it's most
	 * needed, we reserve some dummy space that we can free just
	 * before doing a garbage collection.
	 */
	{
		using namespace File_system;

		Dir_handle db_dir;
		try {
			db_dir = _fs.dir(File_system::Path("/nix/store"), true);
		}
		catch (Node_already_exists) {
			db_dir = _fs.dir(File_system::Path("/nix/store"), false);
		}
		if (reserve_space) {
			File_handle reserve;
			try {
				reserve = _fs.file(db_dir, "reserved", WRITE_ONLY, true);
			}
			catch (Node_already_exists) {
				reserve = _fs.file(db_dir, "reserved", WRITE_ONLY, false);
			}
			enum { RESERVED_SPACE = 1024 * 1024 };
			_fs.truncate(reserve, RESERVED_SPACE);
			_fs.close(reserve);
		} else {
			try {
				_fs.unlink(db_dir, "reserved");
			}
			catch (Lookup_failed) {}
		}
		_fs.close(db_dir);
	}

	/* Check the current database schema. */
	int curSchema = getSchema();
	if (curSchema == 0) {
		/* new store */
		curSchema = nixSchemaVersion;
		openDB(true);
		writeFile(schemaPath, (format("%1%") % nixSchemaVersion).str());
	} else if (curSchema != nixSchemaVersion)
		throw Error(format("current Nix store schema is version %1%, but I only support %2%")
			% curSchema % nixSchemaVersion);
	else
		openDB(false);
}


Store::~Store()
{
	/*
	 * TODO:
	 * Stop the runningSubstituters.
	 */

    try {
        if (fdTempRoots != -1) {
            fdTempRoots.close();
            unlink(fnTempRoots.c_str());
        }
    } catch (...) {
        ignoreException();
    }
}


int Store::getSchema()
{
	int cur(0);
	try {
		if (!string2Int(read_file(schemaPath), cur))
			throw Error(format("‘%1%’ is corrupt") % schemaPath);
	}
	catch (File_system::Lookup_failed) { }
	return cur;
}


void Store::openDB(bool create)
{
    /* Open the Nix database. */
    string dbPath = settings.nixDBPath + "/db.sqlite";
    if (sqlite3_open_v2(dbPath.c_str(), &db.db,
            SQLITE_OPEN_READWRITE | (create ? SQLITE_OPEN_CREATE : 0), 0) != SQLITE_OK)
        throw Error(format("cannot open Nix database ‘%1%’") % dbPath);

    if (sqlite3_busy_timeout(db, 60 * 60 * 1000) != SQLITE_OK)
        throwSQLiteError(db, "setting timeout");

    if (sqlite3_exec(db, "pragma foreign_keys = 1;", 0, 0, 0) != SQLITE_OK)
        throwSQLiteError(db, "enabling foreign keys");

    /* !!! check whether sqlite has been built with foreign key
       support */

    /* Whether SQLite should fsync().  "Normal" synchronous mode
       should be safe enough.  If the user asks for it, don't sync at
       all.  This can cause database corruption if the system
       crashes. */
    string syncMode = settings.fsyncMetadata ? "normal" : "off";
    if (sqlite3_exec(db, ("pragma synchronous = " + syncMode + ";").c_str(), 0, 0, 0) != SQLITE_OK)
        throwSQLiteError(db, "setting synchronous mode");

    /*
     * Set the SQLite journal mode.
     * Upstream uses WAL mode, but WAL is not
     * recommended without file locking support.
     *
     * Truncation only modifies files, whereas 
     * deletion modifies directories and files.
     */
    string mode = "TRUNCATE";
    string prevMode;
    {
        SQLiteStmt stmt;
        stmt.create(db, "pragma main.journal_mode;");
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE)
            prevMode = "NONE";
        else if (rc == SQLITE_ROW)
            prevMode = string((const char *) sqlite3_column_text(stmt, 0));
        else
            throwSQLiteError(db, "querying journal mode");        
    }
    if (prevMode != mode &&
        sqlite3_exec(db, ("pragma main.journal_mode = " + mode + ";").c_str(), 0, 0, 0) != SQLITE_OK)
        throwSQLiteError(db, "setting journal mode");

    /* Increase the auto-checkpoint interval to 40000 pages.  This
       seems enough to ensure that instantiating the NixOS system
       derivation is done in a single fsync(). */
    if (mode == "wal" && sqlite3_exec(db, "pragma wal_autocheckpoint = 40000;", 0, 0, 0) != SQLITE_OK)
        throwSQLiteError(db, "setting autocheckpoint interval");
    /* Initialise the database schema, if necessary. */
    if (create) {
        const char * schema =
#include "schema.sql.hh"
            ;
        if (sqlite3_exec(db, (const char *) schema, 0, 0, 0) != SQLITE_OK)
            throwSQLiteError(db, "initialising database schema");
    }

    /* Prepare SQL statements. */
    stmtRegisterValidPath.create(db,
        "insert into ValidPaths (path, hash, registrationTime, deriver, narSize) values (?, ?, ?, ?, ?);");
    stmtUpdatePathInfo.create(db,
        "update ValidPaths set narSize = ?, hash = ? where path = ?;");
    stmtAddReference.create(db,
        "insert or replace into Refs (referrer, reference) values (?, ?);");
    stmtQueryPathInfo.create(db,
        "select id, hash, registrationTime, deriver, narSize from ValidPaths where path = ?;");
    stmtQueryReferences.create(db,
        "select path from Refs join ValidPaths on reference = id where referrer = ?;");
    stmtQueryReferrers.create(db,
        "select path from Refs join ValidPaths on referrer = id where reference = (select id from ValidPaths where path = ?);");
    stmtInvalidatePath.create(db,
        "delete from ValidPaths where path = ?;");
    stmtRegisterFailedPath.create(db,
        "insert or ignore into FailedPaths (path, time) values (?, ?);");
    stmtHasPathFailed.create(db,
        "select time from FailedPaths where path = ?;");
    stmtQueryFailedPaths.create(db,
        "select path from FailedPaths;");
    // If the path is a derivation, then clear its outputs.
    stmtClearFailedPath.create(db,
        "delete from FailedPaths where ?1 = '*' or path = ?1 "
        "or path in (select d.path from DerivationOutputs d join ValidPaths v on d.drv = v.id where v.path = ?1);");
    stmtAddDerivationOutput.create(db,
        "insert or replace into DerivationOutputs (drv, id, path) values (?, ?, ?);");
    stmtQueryValidDerivers.create(db,
        "select v.id, v.path from DerivationOutputs d join ValidPaths v on d.drv = v.id where d.path = ?;");
    stmtQueryDerivationOutputs.create(db,
        "select id, path from DerivationOutputs where drv = ?;");
    // Use "path >= ?" with limit 1 rather than "path like '?%'" to
    // ensure efficient lookup.
    stmtQueryPathFromHashPart.create(db,
        "select path from ValidPaths where path >= ? limit 1;");
}


/* To improve purity, users may want to make the Nix store a read-only
   bind mount.  So make the Nix store writable for this process. */
void Store::makeStoreWritable()
{
}


void canonicaliseTimestampAndPermissions(const Path & path)
{
	NOT_IMP;
}


void canonicalisePathMetaData(const Path & path, uid_t fromUid)
{
	NOT_IMP;
}


void Store::checkDerivationOutputs(const Path & drvPath, const Derivation & drv)
{
    string drvName = storePathToName(drvPath);
    assert(isDerivation(drvName));
    drvName = string(drvName, 0, drvName.size() - drvExtension.size());

    if (isFixedOutputDrv(drv)) {
        DerivationOutputs::const_iterator out = drv.outputs.find("out");
        if (out == drv.outputs.end())
            throw Error(format("derivation ‘%1%’ does not have an output named ‘out’") % drvPath);

        bool recursive; HashType ht; Hash h;
        out->second.parseHashInfo(recursive, ht, h);
        Path outPath = makeFixedOutputPath(recursive, ht, h, drvName);

        StringPairs::const_iterator j = drv.env.find("out");
        if (out->second.path != outPath || j == drv.env.end() || j->second != outPath)
            throw Error(format("derivation ‘%1%’ has incorrect output ‘%2%’, should be ‘%3%’")
                % drvPath % out->second.path % outPath);
    }

    else {
        Derivation drvCopy(drv);
        foreach (DerivationOutputs::iterator, i, drvCopy.outputs) {
            i->second.path = "";
            drvCopy.env[i->first] = "";
        }

        Hash h = hashDerivationModulo(*this, drvCopy);

        foreach (DerivationOutputs::const_iterator, i, drv.outputs) {
            Path outPath = makeOutputPath(i->first, h, drvName);
            StringPairs::const_iterator j = drv.env.find(i->first);
            if (i->second.path != outPath || j == drv.env.end() || j->second != outPath)
                throw Error(format("derivation ‘%1%’ has incorrect output ‘%2%’, should be ‘%3%’")
                    % drvPath % i->second.path % outPath);
        }
    }
}


unsigned long long Store::addValidPath(const ValidPathInfo & info, bool checkOutputs)
{
    SQLiteStmtUse use(stmtRegisterValidPath);
    stmtRegisterValidPath.bind(info.path);
    stmtRegisterValidPath.bind("sha256:" + printHash(info.hash));
    stmtRegisterValidPath.bind(info.registrationTime == 0 ? time(0) : info.registrationTime);
    if (info.deriver != "")
        stmtRegisterValidPath.bind(info.deriver);
    else
        stmtRegisterValidPath.bind(); // null
    if (info.narSize != 0)
        stmtRegisterValidPath.bind64(info.narSize);
    else
        stmtRegisterValidPath.bind(); // null
    if (sqlite3_step(stmtRegisterValidPath) != SQLITE_DONE)
        throwSQLiteError(db, format("registering valid path ‘%1%’ in database") % info.path);
    unsigned long long id = sqlite3_last_insert_rowid(db);

    /* If this is a derivation, then store the derivation outputs in
       the database.  This is useful for the garbage collector: it can
       efficiently query whether a path is an output of some
       derivation. */
    if (isDerivation(info.path)) {
        Derivation drv = readDerivation(info.path);

        /* Verify that the output paths in the derivation are correct
           (i.e., follow the scheme for computing output paths from
           derivations).  Note that if this throws an error, then the
           DB transaction is rolled back, so the path validity
           registration above is undone. */
        if (checkOutputs) checkDerivationOutputs(info.path, drv);

        foreach (DerivationOutputs::iterator, i, drv.outputs) {
            SQLiteStmtUse use(stmtAddDerivationOutput);
            stmtAddDerivationOutput.bind(id);
            stmtAddDerivationOutput.bind(i->first);
            stmtAddDerivationOutput.bind(i->second.path);
            if (sqlite3_step(stmtAddDerivationOutput) != SQLITE_DONE)
                throwSQLiteError(db, format("adding derivation output for ‘%1%’ in database") % info.path);
        }
    }

    return id;
}


void Store::addReference(unsigned long long referrer, unsigned long long reference)
{
    SQLiteStmtUse use(stmtAddReference);
    stmtAddReference.bind(referrer);
    stmtAddReference.bind(reference);
    if (sqlite3_step(stmtAddReference) != SQLITE_DONE)
        throwSQLiteError(db, "adding reference to database");
}


void Store::registerFailedPath(const Path & path)
{
    retry_sqlite {
        SQLiteStmtUse use(stmtRegisterFailedPath);
        stmtRegisterFailedPath.bind(path);
        stmtRegisterFailedPath.bind(time(0));
        if (sqlite3_step(stmtRegisterFailedPath) != SQLITE_DONE)
            throwSQLiteError(db, format("registering failed path ‘%1%’") % path);
    } end_retry_sqlite;
}


bool Store::hasPathFailed(const Path & path)
{
    retry_sqlite {
        SQLiteStmtUse use(stmtHasPathFailed);
        stmtHasPathFailed.bind(path);
        int res = sqlite3_step(stmtHasPathFailed);
        if (res != SQLITE_DONE && res != SQLITE_ROW)
            throwSQLiteError(db, "querying whether path failed");
        return res == SQLITE_ROW;
    } end_retry_sqlite;
}


PathSet Store::queryFailedPaths()
{
    retry_sqlite {
        SQLiteStmtUse use(stmtQueryFailedPaths);

        PathSet res;
        int r;
        while ((r = sqlite3_step(stmtQueryFailedPaths)) == SQLITE_ROW) {
            const char * s = (const char *) sqlite3_column_text(stmtQueryFailedPaths, 0);
            assert(s);
            res.insert(s);
        }

        if (r != SQLITE_DONE)
            throwSQLiteError(db, "error querying failed paths");

        return res;
    } end_retry_sqlite;
}


void Store::clearFailedPaths(const PathSet & paths)
{
    retry_sqlite {
        SQLiteTxn txn(db);

        foreach (PathSet::const_iterator, i, paths) {
            SQLiteStmtUse use(stmtClearFailedPath);
            stmtClearFailedPath.bind(*i);
            if (sqlite3_step(stmtClearFailedPath) != SQLITE_DONE)
                throwSQLiteError(db, format("clearing failed path ‘%1%’ in database") % *i);
        }

        txn.commit();
    } end_retry_sqlite;
}


Hash parseHashField(const Path & path, const string & s)
{
    string::size_type colon = s.find(':');
    if (colon == string::npos)
        throw Error(format("corrupt hash ‘%1%’ in valid-path entry for ‘%2%’")
            % s % path);
    HashType ht = parseHashType(string(s, 0, colon));
    if (ht == htUnknown)
        throw Error(format("unknown hash type ‘%1%’ in valid-path entry for ‘%2%’")
            % string(s, 0, colon) % path);
    return parseHash(ht, string(s, colon + 1));
}


ValidPathInfo Store::queryPathInfo(const Path & path)
{
    ValidPathInfo info;
    info.path = path;

    assertStorePath(path);

    retry_sqlite {

        /* Get the path info. */
        SQLiteStmtUse use1(stmtQueryPathInfo);

        stmtQueryPathInfo.bind(path);

        int r = sqlite3_step(stmtQueryPathInfo);
        if (r == SQLITE_DONE) throw Error(format("path ‘%1%’ is not valid") % path);
        if (r != SQLITE_ROW) throwSQLiteError(db, "querying path in database");

        info.id = sqlite3_column_int(stmtQueryPathInfo, 0);

        const char * s = (const char *) sqlite3_column_text(stmtQueryPathInfo, 1);
        assert(s);
        info.hash = parseHashField(path, s);

        info.registrationTime = sqlite3_column_int(stmtQueryPathInfo, 2);

        s = (const char *) sqlite3_column_text(stmtQueryPathInfo, 3);
        if (s) info.deriver = s;

        /* Note that narSize = NULL yields 0. */
        info.narSize = sqlite3_column_int64(stmtQueryPathInfo, 4);

        /* Get the references. */
        SQLiteStmtUse use2(stmtQueryReferences);

        stmtQueryReferences.bind(info.id);

        while ((r = sqlite3_step(stmtQueryReferences)) == SQLITE_ROW) {
            s = (const char *) sqlite3_column_text(stmtQueryReferences, 0);
            assert(s);
            info.references.insert(s);
        }

        if (r != SQLITE_DONE)
            throwSQLiteError(db, format("error getting references of ‘%1%’") % path);

        return info;
    } end_retry_sqlite;
}


/* Update path info in the database.  Currently only updates the
   narSize field. */
void Store::updatePathInfo(const ValidPathInfo & info)
{
    SQLiteStmtUse use(stmtUpdatePathInfo);
    if (info.narSize != 0)
        stmtUpdatePathInfo.bind64(info.narSize);
    else
        stmtUpdatePathInfo.bind(); // null
    stmtUpdatePathInfo.bind("sha256:" + printHash(info.hash));
    stmtUpdatePathInfo.bind(info.path);
    if (sqlite3_step(stmtUpdatePathInfo) != SQLITE_DONE)
        throwSQLiteError(db, format("updating info of path ‘%1%’ in database") % info.path);
}


unsigned long long Store::queryValidPathId(const Path & path)
{
    SQLiteStmtUse use(stmtQueryPathInfo);
    stmtQueryPathInfo.bind(path);
    int res = sqlite3_step(stmtQueryPathInfo);
    if (res == SQLITE_ROW) return sqlite3_column_int(stmtQueryPathInfo, 0);
    if (res == SQLITE_DONE) throw Error(format("path ‘%1%’ is not valid") % path);
    throwSQLiteError(db, "querying path in database");
}


bool Store::isValidPath_(const Path & path)
{
    SQLiteStmtUse use(stmtQueryPathInfo);
    stmtQueryPathInfo.bind(path);
    int res = sqlite3_step(stmtQueryPathInfo);
    if (res != SQLITE_DONE && res != SQLITE_ROW)
        throwSQLiteError(db, "querying path in database");
    return res == SQLITE_ROW;
}


bool Store::isValidPath(const Path & path)
{
    retry_sqlite {
        return isValidPath_(path);
    } end_retry_sqlite;
}


PathSet Store::queryValidPaths(const PathSet & paths)
{
    retry_sqlite {
        PathSet res;
        foreach (PathSet::const_iterator, i, paths)
            if (isValidPath_(*i)) res.insert(*i);
        return res;
    } end_retry_sqlite;
}


PathSet Store::queryAllValidPaths()
{
    retry_sqlite {
        SQLiteStmt stmt;
        stmt.create(db, "select path from ValidPaths");

        PathSet res;
        int r;
        while ((r = sqlite3_step(stmt)) == SQLITE_ROW) {
            const char * s = (const char *) sqlite3_column_text(stmt, 0);
            assert(s);
            res.insert(s);
        }

        if (r != SQLITE_DONE)
            throwSQLiteError(db, "error getting valid paths");

        return res;
    } end_retry_sqlite;
}


void Store::queryReferences(const Path & path,
    PathSet & references)
{
    ValidPathInfo info = queryPathInfo(path);
    references.insert(info.references.begin(), info.references.end());
}


void Store::queryReferrers_(const Path & path, PathSet & referrers)
{
    SQLiteStmtUse use(stmtQueryReferrers);

    stmtQueryReferrers.bind(path);

    int r;
    while ((r = sqlite3_step(stmtQueryReferrers)) == SQLITE_ROW) {
        const char * s = (const char *) sqlite3_column_text(stmtQueryReferrers, 0);
        assert(s);
        referrers.insert(s);
    }

    if (r != SQLITE_DONE)
        throwSQLiteError(db, format("error getting references of ‘%1%’") % path);
}


void Store::queryReferrers(const Path & path, PathSet & referrers)
{
    assertStorePath(path);
    retry_sqlite {
        queryReferrers_(path, referrers);
    } end_retry_sqlite;
}


Path Store::queryDeriver(const Path & path)
{
    return queryPathInfo(path).deriver;
}


PathSet Store::queryValidDerivers(const Path & path)
{
    assertStorePath(path);

    retry_sqlite {
        SQLiteStmtUse use(stmtQueryValidDerivers);
        stmtQueryValidDerivers.bind(path);

        PathSet derivers;
        int r;
        while ((r = sqlite3_step(stmtQueryValidDerivers)) == SQLITE_ROW) {
            const char * s = (const char *) sqlite3_column_text(stmtQueryValidDerivers, 1);
            assert(s);
            derivers.insert(s);
        }

        if (r != SQLITE_DONE)
            throwSQLiteError(db, format("error getting valid derivers of ‘%1%’") % path);

        return derivers;
    } end_retry_sqlite;
}


PathSet Store::queryDerivationOutputs(const Path & path)
{
    retry_sqlite {
        SQLiteStmtUse use(stmtQueryDerivationOutputs);
        stmtQueryDerivationOutputs.bind(queryValidPathId(path));

        PathSet outputs;
        int r;
        while ((r = sqlite3_step(stmtQueryDerivationOutputs)) == SQLITE_ROW) {
            const char * s = (const char *) sqlite3_column_text(stmtQueryDerivationOutputs, 1);
            assert(s);
            outputs.insert(s);
        }

        if (r != SQLITE_DONE)
            throwSQLiteError(db, format("error getting outputs of ‘%1%’") % path);

        return outputs;
    } end_retry_sqlite;
}


StringSet Store::queryDerivationOutputNames(const Path & path)
{
    retry_sqlite {
        SQLiteStmtUse use(stmtQueryDerivationOutputs);
        stmtQueryDerivationOutputs.bind(queryValidPathId(path));

        StringSet outputNames;
        int r;
        while ((r = sqlite3_step(stmtQueryDerivationOutputs)) == SQLITE_ROW) {
            const char * s = (const char *) sqlite3_column_text(stmtQueryDerivationOutputs, 0);
            assert(s);
            outputNames.insert(s);
        }

        if (r != SQLITE_DONE)
            throwSQLiteError(db, format("error getting output names of ‘%1%’") % path);

        return outputNames;
    } end_retry_sqlite;
}


Path Store::queryPathFromHashPart(const string & hashPart)
{
    if (hashPart.size() != 32) throw Error("invalid hash part");

    Path prefix = settings.nixStore + "/" + hashPart;

    retry_sqlite {
        SQLiteStmtUse use(stmtQueryPathFromHashPart);
        stmtQueryPathFromHashPart.bind(prefix);

        int res = sqlite3_step(stmtQueryPathFromHashPart);
        if (res == SQLITE_DONE) return "";
        if (res != SQLITE_ROW) throwSQLiteError(db, "finding path in database");

        const char * s = (const char *) sqlite3_column_text(stmtQueryPathFromHashPart, 0);
        return s && prefix.compare(0, prefix.size(), s, prefix.size()) == 0 ? s : "";
    } end_retry_sqlite;
}


void Store::setSubstituterEnv()
{
    if (didSetSubstituterEnv) return;

    /* Pass configuration options (including those overridden with
       --option) to substituters. */
    setenv("_NIX_OPTIONS", settings.pack().c_str(), 1);

    didSetSubstituterEnv = true;
}


Derivation Store::derivationFromPath(const Path & drvPath)
{
	assertStorePath(drvPath);
	ensurePath(drvPath);
	return readDerivation(drvPath);
}

void Store::computeFSClosure(const Path & path, PathSet & paths,
                             bool flipDirection,
                             bool includeOutputs,
                             bool includeDerivers)
{
    if (paths.find(path) != paths.end()) return;
    paths.insert(path);

    PathSet edges;

    if (flipDirection) {
        queryReferrers(path, edges);

        if (includeOutputs) {
            PathSet derivers = queryValidDerivers(path);
            foreach (PathSet::iterator, i, derivers)
                edges.insert(*i);
        }

        if (includeDerivers && isDerivation(path)) {
            PathSet outputs = queryDerivationOutputs(path);
            foreach (PathSet::iterator, i, outputs)
                if (isValidPath(*i) && queryDeriver(*i) == path)
                    edges.insert(*i);
        }

    } else {
        queryReferences(path, edges);

        if (includeOutputs && isDerivation(path)) {
            PathSet outputs = queryDerivationOutputs(path);
            foreach (PathSet::iterator, i, outputs)
                if (isValidPath(*i)) edges.insert(*i);
        }

        if (includeDerivers) {
            Path deriver = queryDeriver(path);
            if (isValidPath(deriver)) edges.insert(deriver);
        }
    }

    foreach (PathSet::iterator, i, edges)
        computeFSClosure(*i, paths, flipDirection, includeOutputs, includeDerivers);
}


void Store::startSubstituter(const Path & substituter, RunningSubstituter & run)
{
	PWRN("%s is a dummy, substitution not implemented", __func__);
}


/* Read a line from the substituter's stdout, while also processing
   its stderr. */
string Store::getLineFromSubstituter(RunningSubstituter & run)
{
	PWRN("%s is a dummy, substitution not implemented", __func__);
	return "";
}


template<class T> T Store::getIntLineFromSubstituter(RunningSubstituter & run)
{
    string s = getLineFromSubstituter(run);
    T res;
    if (!string2Int(s, res)) throw Error("integer expected from stream");
    return res;
}


PathSet Store::querySubstitutablePaths(const PathSet & paths)
{
    PathSet res;
    foreach (Paths::iterator, i, settings.substituters) {
        if (res.size() == paths.size()) break;
        RunningSubstituter & run(runningSubstituters[*i]);
        startSubstituter(*i, run);
        if (run.disabled) continue;
        string s = "have ";
        foreach (PathSet::const_iterator, j, paths)
            if (res.find(*j) == res.end()) { s += *j; s += " "; }
        writeLine(run.to, s);
        while (true) {
            /* FIXME: we only read stderr when an error occurs, so
               substituters should only write (short) messages to
               stderr when they fail.  I.e. they shouldn't write debug
               output. */
            Path path = getLineFromSubstituter(run);
            if (path == "") break;
            res.insert(path);
        }
    }
    return res;
}


void Store::querySubstitutablePathInfos(const Path & substituter,
    PathSet & paths, SubstitutablePathInfos & infos)
{
    RunningSubstituter & run(runningSubstituters[substituter]);
    startSubstituter(substituter, run);
    if (run.disabled) return;

    string s = "info ";
    foreach (PathSet::const_iterator, i, paths)
        if (infos.find(*i) == infos.end()) { s += *i; s += " "; }
    writeLine(run.to, s);

    while (true) {
        Path path = getLineFromSubstituter(run);
        if (path == "") break;
        if (paths.find(path) == paths.end())
            throw Error(format("got unexpected path ‘%1%’ from substituter") % path);
        paths.erase(path);
        SubstitutablePathInfo & info(infos[path]);
        info.deriver = getLineFromSubstituter(run);
        if (info.deriver != "") assertStorePath(info.deriver);
        int nrRefs = getIntLineFromSubstituter<int>(run);
        while (nrRefs--) {
            Path p = getLineFromSubstituter(run);
            assertStorePath(p);
            info.references.insert(p);
        }
        info.downloadSize = getIntLineFromSubstituter<long long>(run);
        info.narSize = getIntLineFromSubstituter<long long>(run);
    }
}


void Store::querySubstitutablePathInfos(const PathSet & paths,
    SubstitutablePathInfos & infos)
{
    PathSet todo = paths;
    foreach (Paths::iterator, i, settings.substituters) {
        if (todo.empty()) break;
        querySubstitutablePathInfos(*i, todo, infos);
    }
}


Hash Store::queryPathHash(const Path & path)
{
    return queryPathInfo(path).hash;
}


void Store::registerValidPath(const ValidPathInfo & info)
{
    ValidPathInfos infos;
    infos.push_back(info);
    registerValidPaths(infos);
}


void Store::registerValidPaths(const ValidPathInfos & infos)
{
    /* SQLite will fsync by default, but the new valid paths may not be fsync-ed.
     * So some may want to fsync them before registering the validity, at the
     * expense of some speed of the path registering operation. */
    if (settings.syncBeforeRegistering) sync();

    retry_sqlite {
        SQLiteTxn txn(db);
        PathSet paths;

        foreach (ValidPathInfos::const_iterator, i, infos) {
            assert(i->hash.type == htSHA256);
            if (isValidPath_(i->path))
                updatePathInfo(*i);
            else
                addValidPath(*i, false);
            paths.insert(i->path);
        }

        foreach (ValidPathInfos::const_iterator, i, infos) {
            unsigned long long referrer = queryValidPathId(i->path);
            foreach (PathSet::iterator, j, i->references)
                addReference(referrer, queryValidPathId(*j));
        }

        /* Check that the derivation outputs are correct.  We can't do
           this in addValidPath() above, because the references might
           not be valid yet. */
        foreach (ValidPathInfos::const_iterator, i, infos)
            if (isDerivation(i->path)) {
                // FIXME: inefficient; we already loaded the
                // derivation in addValidPath().
                Derivation drv = readDerivation(i->path);
                checkDerivationOutputs(i->path, drv);
            }

        /* Do a topological sort of the paths.  This will throw an
           error if a cycle is detected and roll back the
           transaction.  Cycles can only occur when a derivation
           has multiple outputs. */
        topoSortPaths(*this, paths);

        txn.commit();
    } end_retry_sqlite;
}


/* Invalidate a path.  The caller is responsible for checking that
   there are no referrers. */
void Store::invalidatePath(const Path & path)
{
    debug(format("invalidating path ‘%1%’") % path);

    drvHashes.erase(path);

    SQLiteStmtUse use(stmtInvalidatePath);

    stmtInvalidatePath.bind(path);

    if (sqlite3_step(stmtInvalidatePath) != SQLITE_DONE)
        throwSQLiteError(db, format("invalidating path ‘%1%’ in database") % path);

    /* Note that the foreign key constraints on the Refs table take
       care of deleting the references entries for `path'. */
}


Path Store::addToStoreFromDump(const string & dump, const string & name,
    bool recursive, HashType hashAlgo, bool repair)
{
    Hash h = hashString(hashAlgo, dump);

    Path dstPath = makeFixedOutputPath(recursive, hashAlgo, h, name);

    addTempRoot(dstPath);

    if (repair || !isValidPath(dstPath)) {

        /* The first check above is an optimisation to prevent
           unnecessary lock acquisition. */

        PathLocks outputLock(singleton<PathSet, Path>(dstPath));

        if (repair || !isValidPath(dstPath)) {

            if (path_exists(dstPath)) delete_path(dstPath);

            if (recursive) {
                StringSource source(dump);
                restorePath(dstPath, source);
            } else
                writeFile(dstPath, dump);

            canonicalisePathMetaData(dstPath, -1);

            /* Register the SHA-256 hash of the NAR serialisation of
               the path in the database.  We may just have computed it
               above (if called with recursive == true and hashAlgo ==
               sha256); otherwise, compute it here. */
            HashResult hash;
            if (recursive) {
                hash.first = hashAlgo == htSHA256 ? h : hashString(htSHA256, dump);
                hash.second = dump.size();
            } else
                hash = hashPath(htSHA256, dstPath);

            optimisePath(dstPath); // FIXME: combine with hashPath()

            ValidPathInfo info;
            info.path = dstPath;
            info.hash = hash.first;
            info.narSize = hash.second;
            registerValidPath(info);
        }

        outputLock.setDeletion(true);
    }

    return dstPath;
}


Path Store::addToStore(const Path & _srcPath,
    bool recursive, HashType hashAlgo, PathFilter & filter, bool repair)
{
    Path srcPath(absPath(_srcPath));
    debug(format("adding ‘%1%’ to the store") % srcPath);

    /* Read the whole path into memory. This is not a very scalable
       method for very large paths, but `copyPath' is mainly used for
       small files. */
    StringSink sink;
    if (recursive)
        dumpPath(srcPath, sink, filter);
    else
        sink.s = read_file(srcPath);

    return addToStoreFromDump(sink.s, baseNameOf(srcPath), recursive, hashAlgo, repair);
}


Path Store::addTextToStore(const string & name, const string & s,
    const PathSet & references, bool repair)
{
    Path dstPath = computeStorePathForText(name, s, references);

    addTempRoot(dstPath);

    if (repair || !isValidPath(dstPath)) {

        PathLocks outputLock(singleton<PathSet, Path>(dstPath));

        if (repair || !isValidPath(dstPath)) {

            if (path_exists(dstPath)) delete_path(dstPath);

            writeFile(dstPath, s);

            canonicalisePathMetaData(dstPath, -1);

            HashResult hash = hashPath(htSHA256, dstPath);

            optimisePath(dstPath);

            ValidPathInfo info;
            info.path = dstPath;
            info.hash = hash.first;
            info.narSize = hash.second;
            info.references = references;
            registerValidPath(info);
        }

        outputLock.setDeletion(true);
    }

    return dstPath;
}


struct HashAndWriteSink : Sink
{
    Sink & writeSink;
    HashSink hashSink;
    HashAndWriteSink(Sink & writeSink) : writeSink(writeSink), hashSink(htSHA256)
    {
    }
    virtual void operator () (const unsigned char * data, size_t len)
    {
        writeSink(data, len);
        hashSink(data, len);
    }
    Hash currentHash()
    {
        return hashSink.currentHash().first;
    }
};


#define EXPORT_MAGIC 0x4558494e


static void checkSecrecy(const Path & path)
{
    struct stat st;
    if (stat(path.c_str(), &st))
        throw SysError(format("getting status of ‘%1%’") % path);
    if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0)
        throw Error(format("file ‘%1%’ should be secret (inaccessible to everybody else)!") % path);
}


void Store::exportPath(const Path & path, bool sign,
    Sink & sink)
{
    assertStorePath(path);

    printMsg(lvlInfo, format("exporting path ‘%1%’") % path);

    if (!isValidPath(path))
        throw Error(format("path ‘%1%’ is not valid") % path);

    HashAndWriteSink hashAndWriteSink(sink);

    dumpPath(path, hashAndWriteSink);

    /* Refuse to export paths that have changed.  This prevents
       filesystem corruption from spreading to other machines.
       Don't complain if the stored hash is zero (unknown). */
    Hash hash = hashAndWriteSink.currentHash();
    Hash storedHash = queryPathHash(path);
    if (hash != storedHash && storedHash != Hash(storedHash.type))
        throw Error(format("hash of path ‘%1%’ has changed from ‘%2%’ to ‘%3%’!") % path
            % printHash(storedHash) % printHash(hash));

    writeInt(EXPORT_MAGIC, hashAndWriteSink);

    writeString(path, hashAndWriteSink);

    PathSet references;
    queryReferences(path, references);
    writeStrings(references, hashAndWriteSink);

    Path deriver = queryDeriver(path);
    writeString(deriver, hashAndWriteSink);

    if (sign) {
        Hash hash = hashAndWriteSink.currentHash();

        writeInt(1, hashAndWriteSink);

        Path tmpDir = createTempDir();
        AutoDelete delTmp(tmpDir);
        Path hashFile = tmpDir + "/hash";
        writeFile(hashFile, printHash(hash));

        Path secretKey = settings.nixConfDir + "/signing-key.sec";
        checkSecrecy(secretKey);

        Strings args;
        args.push_back("rsautl");
        args.push_back("-sign");
        args.push_back("-inkey");
        args.push_back(secretKey);
        args.push_back("-in");
        args.push_back(hashFile);
        string signature = runProgram(OPENSSL_PATH, true, args);

        writeString(signature, hashAndWriteSink);

    } else
        writeInt(0, hashAndWriteSink);
}


struct HashAndReadSource : Source
{
    Source & readSource;
    HashSink hashSink;
    bool hashing;
    HashAndReadSource(Source & readSource) : readSource(readSource), hashSink(htSHA256)
    {
        hashing = true;
    }
    size_t read(unsigned char * data, size_t len)
    {
        size_t n = readSource.read(data, len);
        if (hashing) hashSink(data, n);
        return n;
    }
};


/* Create a temporary directory in the store that won't be
   garbage-collected. */
Path Store::createTempDirInStore()
{
    Path tmpDir;
    do {
        /* There is a slight possibility that `tmpDir' gets deleted by
           the GC between createTempDir() and addTempRoot(), so repeat
           until `tmpDir' exists. */
        tmpDir = createTempDir(settings.nixStore);
        addTempRoot(tmpDir);
    } while (!path_exists(tmpDir));
    return tmpDir;
}


Path Store::importPath(bool requireSignature, Source & source)
{
    HashAndReadSource hashAndReadSource(source);

    /* We don't yet know what store path this archive contains (the
       store path follows the archive data proper), and besides, we
       don't know yet whether the signature is valid. */
    Path tmpDir = createTempDirInStore();
    AutoDelete delTmp(tmpDir);
    Path unpacked = tmpDir + "/unpacked";

    restorePath(unpacked, hashAndReadSource);

    unsigned int magic = readInt(hashAndReadSource);
    if (magic != EXPORT_MAGIC)
        throw Error("Nix archive cannot be imported; wrong format");

    Path dstPath = readStorePath(hashAndReadSource);

    PathSet references = readStorePaths(hashAndReadSource);

    Path deriver = readString(hashAndReadSource);
    if (deriver != "") assertStorePath(deriver);

    Hash hash = hashAndReadSource.hashSink.finish().first;
    hashAndReadSource.hashing = false;

    bool haveSignature = readInt(hashAndReadSource) == 1;

    if (requireSignature && !haveSignature)
        throw Error(format("imported archive of ‘%1%’ lacks a signature") % dstPath);

    if (haveSignature) {
        string signature = readString(hashAndReadSource);

        if (requireSignature) {
            Path sigFile = tmpDir + "/sig";
            writeFile(sigFile, signature);

            Strings args;
            args.push_back("rsautl");
            args.push_back("-verify");
            args.push_back("-inkey");
            args.push_back(settings.nixConfDir + "/signing-key.pub");
            args.push_back("-pubin");
            args.push_back("-in");
            args.push_back(sigFile);
            string hash2 = runProgram(OPENSSL_PATH, true, args);

            /* Note: runProgram() throws an exception if the signature
               is invalid. */

            if (printHash(hash) != hash2)
                throw Error(
                    "signed hash doesn't match actual contents of imported "
                    "archive; archive could be corrupt, or someone is trying "
                    "to import a Trojan horse");
        }
    }

    /* Do the actual import. */

    /* !!! way too much code duplication with addTextToStore() etc. */
    addTempRoot(dstPath);

    if (!isValidPath(dstPath)) {

        PathLocks outputLock;

        /* Lock the output path.  But don't lock if we're being called
           from a build hook (whose parent process already acquired a
           lock on this path). */
        Strings locksHeld = tokenizeString<Strings>(getEnv("NIX_HELD_LOCKS"));
        if (find(locksHeld.begin(), locksHeld.end(), dstPath) == locksHeld.end())
            outputLock.lockPaths(singleton<PathSet, Path>(dstPath));

        if (!isValidPath(dstPath)) {

            if (path_exists(dstPath)) delete_path(dstPath);

            if (rename(unpacked.c_str(), dstPath.c_str()) == -1)
                throw SysError(format("cannot move ‘%1%’ to ‘%2%’")
                    % unpacked % dstPath);

            canonicalisePathMetaData(dstPath, -1);

            /* !!! if we were clever, we could prevent the hashPath()
               here. */
            HashResult hash = hashPath(htSHA256, dstPath);

            optimisePath(dstPath); // FIXME: combine with hashPath()

            ValidPathInfo info;
            info.path = dstPath;
            info.hash = hash.first;
            info.narSize = hash.second;
            info.references = references;
            info.deriver = deriver != "" && isValidPath(deriver) ? deriver : "";
            registerValidPath(info);
        }

        outputLock.setDeletion(true);
    }

    return dstPath;
}


Paths Store::importPaths(bool requireSignature, Source & source)
{
    Paths res;
    while (true) {
        unsigned long long n = readLongLong(source);
        if (n == 0) break;
        if (n != 1) throw Error("input doesn't look like something created by ‘nix-store --export’");
        res.push_back(importPath(requireSignature, source));
    }
    return res;
}


void Store::invalidatePathChecked(const Path & path)
{
    assertStorePath(path);

    retry_sqlite {
        SQLiteTxn txn(db);

        if (isValidPath_(path)) {
            PathSet referrers; queryReferrers_(path, referrers);
            referrers.erase(path); /* ignore self-references */
            if (!referrers.empty())
                throw PathInUse(format("cannot delete path ‘%1%’ because it is in use by %2%")
                    % path % showPaths(referrers));
            invalidatePath(path);
        }

        txn.commit();
    } end_retry_sqlite;
}


bool Store::verifyStore(bool checkContents, bool repair)
{
    printMsg(lvlError, format("reading the Nix store..."));

    bool errors = false;

    /* Acquire the global GC lock to prevent a garbage collection. */
    AutoCloseFD fdGCLock = openGCLock(ltWrite);

    PathSet store;
    for (auto & i : readDirectory(settings.nixStore)) store.insert(i.name);

    /* Check whether all valid paths actually exist. */
    printMsg(lvlInfo, "checking path existence...");

    PathSet validPaths2 = queryAllValidPaths(), validPaths, done;

    foreach (PathSet::iterator, i, validPaths2)
        verifyPath(*i, store, done, validPaths, repair, errors);

    /* Release the GC lock so that checking content hashes (which can
       take ages) doesn't block the GC or builds. */
    fdGCLock.close();

    /* Optionally, check the content hashes (slow). */
    if (checkContents) {
        printMsg(lvlInfo, "checking hashes...");

        Hash nullHash(htSHA256);

        foreach (PathSet::iterator, i, validPaths) {
            try {
                ValidPathInfo info = queryPathInfo(*i);

                /* Check the content hash (optionally - slow). */
                printMsg(lvlTalkative, format("checking contents of ‘%1%’") % *i);
                HashResult current = hashPath(info.hash.type, *i);

                if (info.hash != nullHash && info.hash != current.first) {
                    printMsg(lvlError, format("path ‘%1%’ was modified! "
                            "expected hash ‘%2%’, got ‘%3%’")
                        % *i % printHash(info.hash) % printHash(current.first));
                    if (repair) repairPath(*i); else errors = true;
                } else {

                    bool update = false;

                    /* Fill in missing hashes. */
                    if (info.hash == nullHash) {
                        printMsg(lvlError, format("fixing missing hash on ‘%1%’") % *i);
                        info.hash = current.first;
                        update = true;
                    }

                    /* Fill in missing narSize fields (from old stores). */
                    if (info.narSize == 0) {
                        printMsg(lvlError, format("updating size field on ‘%1%’ to %2%") % *i % current.second);
                        info.narSize = current.second;
                        update = true;
                    }

                    if (update) updatePathInfo(info);

                }

            } catch (Error & e) {
                /* It's possible that the path got GC'ed, so ignore
                   errors on invalid paths. */
                if (isValidPath(*i))
                    printMsg(lvlError, format("error: %1%") % e.msg());
                else
                    printMsg(lvlError, format("warning: %1%") % e.msg());
                errors = true;
            }
        }
    }

    return errors;
}


void Store::verifyPath(const Path & path, const PathSet & store,
    PathSet & done, PathSet & validPaths, bool repair, bool & errors)
{
    checkInterrupt();

    if (done.find(path) != done.end()) return;
    done.insert(path);

    if (!isStorePath(path)) {
        printMsg(lvlError, format("path ‘%1%’ is not in the Nix store") % path);
        invalidatePath(path);
        return;
    }

    if (store.find(baseNameOf(path)) == store.end()) {
        /* Check any referrers first.  If we can invalidate them
           first, then we can invalidate this path as well. */
        bool canInvalidate = true;
        PathSet referrers; queryReferrers(path, referrers);
        foreach (PathSet::iterator, i, referrers)
            if (*i != path) {
                verifyPath(*i, store, done, validPaths, repair, errors);
                if (validPaths.find(*i) != validPaths.end())
                    canInvalidate = false;
            }

        if (canInvalidate) {
            printMsg(lvlError, format("path ‘%1%’ disappeared, removing from database...") % path);
            invalidatePath(path);
        } else {
            printMsg(lvlError, format("path ‘%1%’ disappeared, but it still has valid referrers!") % path);
            if (repair)
                try {
                    repairPath(path);
                } catch (Error & e) {
                    printMsg(lvlError, format("warning: %1%") % e.msg());
                    errors = true;
                }
            else errors = true;
        }

        return;
    }

    validPaths.insert(path);
}


bool Store::pathContentsGood(const Path & path)
{
    std::map<Path, bool>::iterator i = pathContentsGoodCache.find(path);
    if (i != pathContentsGoodCache.end()) return i->second;
    printMsg(lvlInfo, format("checking path ‘%1%’...") % path);
    ValidPathInfo info = queryPathInfo(path);
    bool res;
    if (!path_exists(path))
        res = false;
    else {
        HashResult current = hashPath(info.hash.type, path);
        Hash nullHash(htSHA256);
        res = info.hash == nullHash || info.hash == current.first;
    }
    pathContentsGoodCache[path] = res;
    if (!res) printMsg(lvlError, format("path ‘%1%’ is corrupted or missing!") % path);
    return res;
}


void Store::markContentsGood(const Path & path)
{
    pathContentsGoodCache[path] = true;
}


/* Functions for upgrading from the pre-SQLite database. */

PathSet Store::queryValidPathsOld()
{
    PathSet paths;
    for (auto & i : readDirectory(settings.nixDBPath + "/info"))
        if (i.name.at(0) != '.') paths.insert(settings.nixStore + "/" + i.name);
    return paths;
}


ValidPathInfo Store::queryPathInfoOld(const Path & path)
{
    ValidPathInfo res;
    res.path = path;

    /* Read the info file. */
    string baseName = baseNameOf(path);
    Path infoFile = (format("%1%/info/%2%") % settings.nixDBPath % baseName).str();
    if (!path_exists(infoFile))
        throw Error(format("path ‘%1%’ is not valid") % path);
    string info = read_file(infoFile);

    /* Parse it. */
    Strings lines = tokenizeString<Strings>(info, "\n");

    foreach (Strings::iterator, i, lines) {
        string::size_type p = i->find(':');
        if (p == string::npos)
            throw Error(format("corrupt line in ‘%1%’: %2%") % infoFile % *i);
        string name(*i, 0, p);
        string value(*i, p + 2);
        if (name == "References") {
            Strings refs = tokenizeString<Strings>(value, " ");
            res.references = PathSet(refs.begin(), refs.end());
        } else if (name == "Deriver") {
            res.deriver = value;
        } else if (name == "Hash") {
            res.hash = parseHashField(path, value);
        } else if (name == "Registered-At") {
            int n = 0;
            string2Int(value, n);
            res.registrationTime = n;
        }
    }

    return res;
}


void Store::vacuumDB()
{
    if (sqlite3_exec(db, "vacuum;", 0, 0, 0) != SQLITE_OK)
        throwSQLiteError(db, "vacuuming SQLite database");
}


File_system::Dir_handle Store::dir_of(const char *path, bool create)
{
	using namespace File_system;
		int i;
		for (i = Genode::strlen(path); i != 0; --i)
			if (path[i] == '/')
				break;

		if (i) {
			char path_[++i];
			Genode::memcpy(path_, path, i-1);
			path_[i] = '\0';
			try {
				return _fs.dir(path_, create);
			}
			catch (Lookup_failed) {
				/* Apply this funtion to the parent and close the result. */
				_fs.close(dir_of(path_, true));
				return _fs.dir(path_, create);
			}
			catch (Node_already_exists) {
				return _fs.dir(path_, false);
			}
		}
		return _fs.dir("/", false);
}


void Store::create_symlink(const char *link, const char *target)
{
	using namespace File_system;
	Dir_handle dir; 
	try {
		dir = dir_of(link, false);

		Name link_name(basename(link));

		/* Create the new symlink. */
		char temp[512];
		Genode::snprintf(&temp[0], sizeof(temp), "%s.tmp-%d", link, rand());
		Name temp_name(&temp[0]);
		Symlink_handle symlink = _fs.symlink(dir, temp_name, true);

		int len = Genode::strlen(target);

		File_system::Session::Tx::Source &source = *_fs.tx();
		File_system::Packet_descriptor packet(source.alloc_packet(len),
		                                      0, symlink,
		                                      File_system::Packet_descriptor::WRITE,
		                                      len, 0);

		Genode::memcpy(source.packet_content(packet), target, len);

		source.submit_packet(packet);
		source.get_acked_packet();

		source.release_packet(packet);
		_fs.close(symlink);

		/* Atomically Replace the old one. */
		_fs.move(dir, temp_name, dir, link_name);
		_fs.close(dir);
	}  
	catch (Genode::Exception &e) {
		_fs.close(dir);
		throw e;
	}
}


Path Store::readStorePath(Source & from)
{
    Path path = readString(from);
    assertStorePath(path);
    return path;
}


PathSet Store::readStorePaths(Source & from)
{
    PathSet paths = readStrings<PathSet>(from);
    foreach (typename PathSet::iterator, i, paths) assertStorePath(*i);
    return paths;
}


void Store::optimiseStore()
{
	PERR("%s not implemented", __func__);
}

void Store::optimisePath(const Path & path) { optimiseStore(); }
void Store::optimiseStore(OptimiseStats & stats) { optimiseStore(); }


void Store::assertStorePath(const Path & path)
{
    if (!isStorePath(path))
        throw Error(format("path ‘%1%’ is not in the Nix store") % path);
}


bool Store::isInStore(const Path & path)
{
	try {
		File_system::Node_handle node = _fs.node(File_system::Path(path.c_str()));
		_fs.close(node);
	} catch (...) { return false; }
	return true;
}


bool Store::isStorePath(const Path &path)
{
	return isInStore(path)
		&& path.find('/', sizeof("/nix/store") + 1) == Path::npos;
}

LocalStore::LocalStore(bool)
{
	PERR("LocalStore not implemented");
};

void LocalStore::invalidatePathChecked(const Path & path)
{
	PERR("LocalStore not implemented");
};

RemoteStore::RemoteStore()
{
	PERR("RemoteStore not implemented");
};


}
