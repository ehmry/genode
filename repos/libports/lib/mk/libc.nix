/*
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, libc }: with tool;

{ libc-string, libc-locale, libc-stdlib, libc-stdio, libc-gen
, libc-gdtoa, libc-inet, libc-stdtime, libc-regex, libc-compat
, libc-setjmp, ldso-startup }:

let
  sourceDir = ../../src/lib/libc;
in
buildLibrary {
  name = "libc";
  shared = true;
  libs =
    [ libc-string libc-locale libc-stdlib libc-stdio libc-gen
      libc-gdtoa libc-inet libc-stdtime libc-regex libc-compat
      libc-setjmp ldso-startup
    ];

  sources =
    ( fromDir sourceDir [
        "atexit.cc" "dummies.cc" "rlimit.cc" "sysctl.cc"
        "issetugid.cc" "errno.cc" "gai_strerror.cc" 
        "clock_gettime.cc" "gettimeofday.cc" "malloc.cc" 
        "progname.cc" "fd_alloc.cc" "file_operations.cc"
        "plugin.cc" "plugin_registry.cc" "select.cc" "exit.cc"
        "environ.cc" "nanosleep.cc" "libc_mem_alloc.cc" 
        "pread_pwrite.cc" "readv_writev.cc" "poll.cc"
        "libc_pdbg.cc" "vfs_plugin.cc" "rtc.cc"
      ] )
    ++
    ( fromDir "${libc}/src/lib/libc/lib/libc/string" [
        # Files from string library that are not included in
        # libc-raw_string because they depend on the locale
        # library.
        "strcoll.c" "strxfrm.c" "wcscoll.c" "wcsxfrm.c"
      ] );

  ldOpt = 
    [ "--version-script=${sourceDir}/Version.def" ] 
    ++ build.ldOpt;

  propagatedIncludes = [ "${libc}/include/libc" ];
}