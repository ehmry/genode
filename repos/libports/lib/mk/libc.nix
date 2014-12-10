{ genodeEnv, linkSharedLibrary, compileCC, compileLibc, libcSrc
, base, config
, libc-string, libc-locale, libc-stdlib, libc-stdio, libc-gen
, libc-gdtoa, libc-inet, libc-stdtime, libc-regex, libc-compat
, libc-setjmp, ldso-startup }:

let
  sourceDir = ../../src/lib/libc;

  compileCC' = src: compileCC {
    src = (sourceDir+"/${src}");

    ccFlags = [
      # Generate position independent code to allow linking of
      # static libc code into shared libraries
      # (define is evaluated by assembler files)
      "-DPIC"

      # Prevent gcc headers from defining __size_t.
      # This definition is done in machine/_types.h.
      "-D__FreeBSD__=8"

      # Prevent gcc-4.4.5 from generating code for the family of
      # 'sin' and 'cos' functions because the gcc-generated code
      # would actually call 'sincos' or 'sincosf', which is a GNU
      # extension, not provided by our libc.
      "-fno-builtin-sin" "-fno-builtin-cos"
      "-fno-builtin-sinf" "-fno-builtin-cosf"
    ]
    ++ genodeEnv.ccFlags;

    systemIncludes =
      map (p: "${libcSrc}/${p}")
        [ "include/libc"
          "lib/libc/include"
          # Add platform-specific libc headers
          # to standard include search paths
          ( if genodeEnv.isArm    then "include/libc-arm"    else
            if genodeEnv.isx86_32 then "include/libc-i386"  else
            if genodeEnv.isx86_64 then "include/libc-amd64" else
            throw "no libc for ${genodeEnv.system}"
          )
        ]
      ++ [ ../../include/libc-genode sourceDir ];
  };

in
linkSharedLibrary {
  name = "libc";
  libs =
    [ libc-string libc-locale libc-stdlib libc-stdio
      libc-gen libc-gdtoa libc-inet libc-stdtime
      libc-regex libc-compat libc-setjmp
    ];

  objects = map compileCC'
    [ "atexit.cc" "dummies.cc" "rlimit.cc" "sysctl.cc"
      "issetugid.cc" "errno.cc" "gai_strerror.cc"
      "clock_gettime.cc" "gettimeofday.cc" "malloc.cc"
      "progname.cc" "fd_alloc.cc" "file_operations.cc"
      "plugin.cc" "plugin_registry.cc" "select.cc" "exit.cc"
      "environ.cc" "nanosleep.cc" "libc_mem_alloc.cc"
      "pread_pwrite.cc" "readv_writev.cc" "poll.cc"
      "libc_pdbg.cc" "vfs_plugin.cc" "rtc.cc" "dynamic_linker.cc"
    ];

  externalObjects = compileLibc {

    sources = map (fn: "lib/libc/string/"+fn)
      [ # Files from string library that are not included in
        # libc-raw_string because they depend on the locale
        # library.
        "strcoll.c" "strxfrm.c" "wcscoll.c" "wcsxfrm.c"
      ];

    localIncludes = [
      "lib/libc/locale"
      ( if genodeEnv.isx86_32 then "sys/i386/include"  else
        if genodeEnv.isx86_64 then "sys/amd64/include" else
        if genodeEnv.isArm    then "sys/arm/include"   else
        throw "no libc for ${genodeEnv.system}"
      )
      "sys"
    ];

  };

  extraLdFlags = [ "--version-script=${sourceDir}/Version.def" ];

  propagatedFlags =
    [ # Prevent gcc headers from defining __size_t.
      # This definition is done in machine/_types.h.
      "-D__FreeBSD__=8"

      # Prevent gcc-4.4.5 from generating code for the family of 'sin'
      # and 'cos' functions because the gcc-generated code would
      # actually call 'sincos' or 'sincosf', which is a GNU extension,
      # not provided by our libc.
      "-fno-builtin-sin" "-fno-builtin-cos"
      "-fno-builtin-sinf" "-fno-builtin-cosf"
    ];

  propagatedIncludes = map
    (fn: "${libcSrc}/${fn}")
    [ "include/libc"
      ( if genodeEnv.isx86_32 then "include/libc-i386"  else
        if genodeEnv.isx86_64 then "include/libc-amd64" else
        if genodeEnv.isxarm then  "include/libc-arm"    else
        throw "no libc for ${genodeEnv.system}"
      )
    ];
}
