{ linkSharedLibrary, compileCC, compileCCRepo, nixSrc
, libc, stdcxx, libnixformat, libnixstore, libnixutil }:

let sourceRoot = "${nixSrc}/src/libexpr"; in
linkSharedLibrary rec {
  name = "libnixexpr";
  libs = [ libc stdcxx libnixformat libnixstore libnixutil ];
  objects = compileCC { src = ../../lib/libnixutil/genode.cc; };
  externalObjects = compileCCRepo {
    inherit name libs sourceRoot;
    sources = "*.cc";
    includes = [ ./include "${nixSrc}/src" ];
    # Make sure libc's regex.h comes before stdcxx's regex.h.
    extraFlags = map (i: "-I"+i) libc.propagate.systemIncludes;
  };
  propagate.systemIncludes = [ ./include sourceRoot ];
}
