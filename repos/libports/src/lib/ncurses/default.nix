{ linkSharedLibrary, compileCRepo, fromDir, libc, ncursesSrc }:

let
  externalIncludes =
    [ ../../../include/ncurses ncursesSrc.source ncursesSrc.include "${ncursesSrc.include}/ncurses" ];
in
linkSharedLibrary rec {
  name = "ncurses";
  libs = [ libc ];
  externalObjects =
    [ (compileCRepo {
        # files from the 'ncurses/base/' subdirectory
        sources = "${ncursesSrc}/base/*.c";
        filter = "sigaction.c lib_driver.c";
        inherit libs externalIncludes;
      })

      (compileCRepo {
        # files from the 'ncurses/tinfo/' subdirectory
        sources = "${ncursesSrc}/tinfo/*.c";
        filter = "make_hash.c make_keys.c tinfo_driver.c";
        inherit libs externalIncludes;
      })

      (compileCRepo {
        # files from the 'ncurses/tty/' subdirectory
        sources = "${ncursesSrc}/tty/*.c";
        inherit libs externalIncludes;
      })

      (compileCRepo {
        # files from the 'ncurses/trace/' subdirectory
        sources = fromDir
          "${ncursesSrc}/trace"
          [ "lib_trace.c" "varargs.c" "visbuf.c" ];
        inherit libs externalIncludes;
      })

      (compileCRepo {
        sources = "${ncursesSrc}/*.c";
        inherit libs externalIncludes;
      })
    ];
  propagate.externalIncludes = [ ../../../include/ncurses "${ncursesSrc.include}/ncurses" ];
}