{ libcEnv
, libc-string, libc-locale, libc-stdlib, libc-stdio, libc-gen
, libc-gdtoa, libc-inet, libc-stdtime, libc-regex, libc-compat
, libc-setjmp, ldso-startup }:

let
  sourceDir = ../../src/lib/libc;
in
libcEnv.mkLibrary {
  name = "libc";
  shared = true;
  libs =
    [ ldso-startup
      libc-string libc-locale libc-stdlib libc-stdio libc-gen
      libc-gdtoa libc-inet libc-stdtime libc-regex libc-compat
      libc-setjmp
    ];

  srcSh =
    map (fn: "lib/libc/string/"+fn)
      [ # Files from string library that are not included in
        # libc-raw_string because they depend on the locale
        # library.
        "strcoll.c" "strxfrm.c" "wcscoll.c" "wcsxfrm.c"
      ];

  src =
    libcEnv.tool.fromDir sourceDir [
      "atexit.cc" "dummies.cc" "rlimit.cc" "sysctl.cc"
      "issetugid.cc" "errno.cc" "gai_strerror.cc" 
      "clock_gettime.cc" "gettimeofday.cc" "malloc.cc" 
      "progname.cc" "fd_alloc.cc" "file_operations.cc"
      "plugin.cc" "plugin_registry.cc" "select.cc" "exit.cc"
      "environ.cc" "nanosleep.cc" "libc_mem_alloc.cc" 
      "pread_pwrite.cc" "readv_writev.cc" "poll.cc"
      "libc_pdbg.cc" "vfs_plugin.cc" "rtc.cc"
    ];

  ldOpt =
    [ "--version-script=${sourceDir}/Version.def" ] ++ libcEnv.ldOpt;

  incDirSh = [
    "include" "lib/libc/locale"
    ( if libcEnv.isx86_32 then "sys/i386/include"  else
      if libcEnv.isx86_64 then "sys/amd64/include" else
      if libcEnv.isxarm then   "sys/arm/include"   else
      throw "no libc for ${libcEnv.system}"
    )
    "sys"
  ];

  incDir =
    [ sourceDir ]
    ++  (import ../../../base/include { genodeEnv = libcEnv; })
    ++  (import ../../../os/include { genodeEnv = libcEnv; })
    ++  (import ../../include { genodeEnv = libcEnv; });

  preGather = ''
    mkdir mach machine
    touch mach/port.h machine/pcpu.h machine/mutex.h
  '';

  #propagatedIncludes = [ "${libc}/include/libc" ];
}
