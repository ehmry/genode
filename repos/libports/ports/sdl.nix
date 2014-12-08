{ preparePort, fetchurl }:

let
  version = "1.2.13";
in
preparePort {
  name = "sdl-${version}";

  src = fetchurl {
    url = "http://www.libsdl.org/release/SDL-${version}.tar.gz";
    sha256 = "0cp155296d6fy3w31jj481jxl9b43fkm01klyibnna8gsvqrvycl";
  };

  # wipe original 'SDL_config.h' file because
  # it conflicts with out version
  prePatch = "rm include/SDL_config.h";

  patches =
    [ ../src/lib/sdl/SDL_video.patch
      ../src/lib/sdl/SDL_audio.patch
    ];

  patchFlags = "-p3";
}
