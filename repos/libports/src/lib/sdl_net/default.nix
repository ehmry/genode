{ genodeEnv, linkSharedLibrary, compileCRepo, sdl_netSrc, libc, sdl }:

linkSharedLibrary rec {
  name = "sdl_net";
  libs = [ libc sdl ];
  externalObjects = compileCRepo {
    inherit name libs;
    sources = "${sdl_netSrc}/SDLnet*.c";
  };
  propagate.externalIncludes =
    [ "${sdl_netSrc.include}/SDL" sdl_netSrc.include ];
}
