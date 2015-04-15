/* Genode includes */
#include <os/config.h>
#include <base/printf.h>

#include <libutil/util.hh>

#include <cerrno>


namespace nix {


static Path tempName(Path tmpRoot, const Path & prefix, bool includePid,
    int & counter)
{
    /*
     * Hardcode the root of temporary directories at /tmp for now.
     * There may be a better way, but /tmp is easy to manage with FS
     * policy at the parent.
     */

    return (format("/tmp/%1%-%2%") % prefix % counter++).str();
}

Path createTempDir(const Path & tmpRoot, const Path & prefix,
    bool includePid, bool useGlobalCounter, mode_t mode)
{
    static int globalCounter = 0;
    int localCounter = 0;
    int & counter(useGlobalCounter ? globalCounter : localCounter);

    if (mkdir("/tmp", mode))
        if (errno != EEXIST)
            throw SysError("creating directory `/tmp'");

    while (1) {
        checkInterrupt();
        Path tmpDir = tempName(tmpRoot, prefix, includePid, counter);
        if (mkdir(tmpDir.c_str(), mode) == 0) {
            return tmpDir;
        }
        if (errno != EEXIST)
            throw SysError(format("creating directory ‘%1%’") % tmpDir);
    }
}


bool showStats() {
    try {
	return Genode::config()->xml_node().attribute("show-stats").has_value("true");
    }
    catch (Genode::Xml_node::Nonexistent_attribute) { return false; }
}


float cpuTime()
{
    PDBG("not implemented");
    return 0.0;
}


}
