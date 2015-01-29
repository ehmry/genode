{ linkStaticLibrary, compileCC, compileCRepo, sqliteSrc, libc, pthread, jitterentropy
, debug ? true }:

linkStaticLibrary rec {
  name = "sqlite";
  libs = [ libc pthread jitterentropy ];

  objects = compileCC {
    src = ./genode_vfs.cc;
    libs = [ libc jitterentropy ];
    systemIncludes = [ sqliteSrc.include ];
  };

  externalObjects = compileCRepo {
    inherit name libs;
    sourceRoot = sqliteSrc;
    sources = "sqlite3.c";
    extraFlags =
      [ # https://github.com/genodelabs/genode/issues/754
        "-DSQLITE_4_BYTE_ALIGNED_MALLOC"

        # Defenestrate the Unix and Windows OS layers and use our own.
        "-DSQLITE_OS_OTHER=1"
      ] ++
      (if debug then [ "-DSQLITE_DEBUG" ] else []);
  };

  propagate =
    { systemIncludes = [ sqliteSrc.include ];
      runtime.requires = [ "Rtc" "Timer" ];
    };
}
