{ preparePort, fetchurl }:

preparePort rec {
  name = "SDL_net-1.2.8";

  src = fetchurl {
    url = "http://www.libsdl.org/projects/SDL_net/release/${name}.tar.gz";
    sha256 = "1d5c9xqlf4s1c01gzv6cxmg0r621pq9kfgxcg3197xw4p25pljjz";
  };

  patches =
    [ ../src/lib/sdl_net/SDLnet.patch
      ../src/lib/sdl_net/SDL_net.h.patch
    ];

 patchFlags = "-p3";
}
