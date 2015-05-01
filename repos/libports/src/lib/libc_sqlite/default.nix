{ linkStaticLibrary, compileCC, compileCRepo
, sqliteSrc, libc, pthread, jitterentropy
, debug ? false }:

let
  objects = compileCC {
    src = ./libc_sqlite.cc;
    libs = [ libc jitterentropy ];
    includes = [ ../sqlite ]; # common.h
    externalIncludes = [ sqliteSrc.include ];
  };
in
import ../sqlite/common.nix {
  inherit
    linkStaticLibrary compileCC compileCRepo
    sqliteSrc libc pthread jitterentropy debug;
} objects