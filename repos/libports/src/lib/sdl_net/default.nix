{ genodeEnv, linkSharedLibrary, compileCRepo, sdl_netSrc, libc, sdl }:

linkSharedLibrary rec {
  name = "sdl_net";
  libs = [ libc sdl ];
  externalObjects = compileCRepo {
    inherit name libs;
    sourceRoot = sdl_netSrc;
    sources = "SDLnet*.c";
    localIncludes = sdl.propagate.systemIncludes;
  };
  propagate.systemIncludes =
    [ "${sdl_netSrc.include}/SDL" sdl_netSrc.include ];
}
