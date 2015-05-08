{ linkSharedLibrary, compileCC, compileCRepo, fromDir
, readlineSrc, libc }:

linkSharedLibrary rec {
  name = "readline";
  libs = [ libc ];
  propagate.externalIncludes = [ readlineSrc.include ];
  objects = compileCC {
    src = ./genode.cc;
  };
  externalObjects = compileCRepo {
    inherit libs;
    externalIncludes =
      [ ../../../include/readline
        readlineSrc.source
        readlineSrc.include
      ];
    extraFlags =
      [ "-DHAVE_CONFIG_H" "-DRL_LIBRARY_VERSION=\\\"${readlineSrc.version}\\\""
        # dim build noise for contrib code
        "-Wno-unused-but-set-variable"
      ];
    sources = fromDir
      (toString readlineSrc)
      [ "bind.c" "callback.c" "compat.c" "complete.c"
        "readline.c"
        "funmap.c"
        "keymaps.c"
        "parens.c"
        "search.c"
        "rltty.c"
        "isearch.c"
        "display.c"
        "signals.c"
        "util.c"
        "kill.c"
        "undo.c"
        "macro.c"
        "input.c"
        "terminal.c"
        "text.c"
        "nls.c"
        "misc.c"
        "xmalloc.c"
        "history.c"
        "histexpand.c"
        "histfile.c"
        "histsearch.c"
        "shell.c"
        "mbutil.c"
        "tilde.c"
        "vi_mode.c"
      ];
  };
}