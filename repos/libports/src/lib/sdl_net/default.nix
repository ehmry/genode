{ genodeEnv, linkSharedLibrary, compileCRepo, sdl_netSrc, libc, sdl }:

let

in
linkSharedLibrary rec {
  name = "sdl_net";
  libs = [ libc sdl ];
  externalObjects = compileCRepo {
    inherit name;
    sourceRoot = sdl_netSrc;
    sources = "SDLnet*";
    localIncludes = map (d: d+"/SDL") sdl.propagatedIncludes;
    systemIncludes = genodeEnv.tool.propagateIncludes libs;
  };
}
