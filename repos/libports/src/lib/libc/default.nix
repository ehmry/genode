{ genodeEnv, linkSharedLibrary, addPrefix, compileCC
, compileLibc, libcSrc, libcIncludes, libcExternalIncludes
, base, config
, libc-string, libc-locale, libc-stdlib, libc-stdio, libc-gen
, libc-gdtoa, libc-inet, libc-stdtime, libc-regex, libc-compat
, libc-setjmp, ldso-startup }:

let
  includes = [ ../libc ] ++ libcIncludes;
  externalIncludes = libcExternalIncludes;

  compileCC' = src: compileCC {
    inherit src includes externalIncludes;
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
    [ ./atexit.cc ./dummies.cc ./rlimit.cc ./sysctl.cc
      ./issetugid.cc ./errno.cc ./gai_strerror.cc
      ./clock_gettime.cc ./gettimeofday.cc ./malloc.cc
      ./progname.cc ./fd_alloc.cc ./file_operations.cc
      ./plugin.cc ./plugin_registry.cc ./select.cc ./exit.cc
      ./environ.cc ./nanosleep.cc ./libc_mem_alloc.cc
      ./pread_pwrite.cc ./readv_writev.cc ./poll.cc
      ./libc_pdbg.cc ./vfs_plugin.cc ./rtc.cc ./dynamic_linker.cc
    ];

  externalObjects = compileLibc {
    sources = addPrefix "lib/libc/string/"
      [ # Files from string library that are not included in
        # libc-raw_string because they depend on the locale
        # library.
        "strcoll.c" "strxfrm.c" "wcscoll.c" "wcsxfrm.c"
      ];
    inherit includes;
    externalIncludes = [ "${libcSrc}/lib/libc/locale" ];
  };

  extraLdFlags = [ "--version-script=${./Version.def}" ];

  propagate =
    { extraFlags =
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
      includes = libcIncludes;
      inherit externalIncludes;
    };
}
