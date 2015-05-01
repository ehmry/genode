{ linkStaticLibrary, compileCC, compileCRepo
, sqliteSrc, libc, pthread, jitterentropy
, debug }:

objects:

linkStaticLibrary rec {
  name = "sqlite";
  libs = [ libc pthread jitterentropy ];
  inherit objects;

  externalObjects = compileCRepo {
    inherit name libs;
    sources = "${sqliteSrc}/sqlite3.c";
    extraFlags =
      [ # https://github.com/genodelabs/genode/issues/754
        "-DSQLITE_4_BYTE_ALIGNED_MALLOC"

        # Defenestrate the Unix and Windows OS layers and use our own.
        "-DSQLITE_OS_OTHER=1"
      ] ++
      (if debug then [ "-DSQLITE_DEBUG" ] else []);
  };

  propagate =
    { externalIncludes = [ sqliteSrc.include ];
      runtime.requires = [ "Timer" "File_system" ];
    };
}
