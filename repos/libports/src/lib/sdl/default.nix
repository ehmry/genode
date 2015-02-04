{ linkSharedLibrary, compileCC, compileCRepo, fromDir
, sdlSrc, libc, pthread }:

let

  externalIncludes = map
    (fn: "${sdlSrc}/${fn}")
    ( [ "include" ] ++
      (map (d: "src/"+d) [ "audio" "events" "timer" "video" ])
    ) ++ [ sdlSrc.include "${sdlSrc.include}/SDL" ];

  libs = [ libc pthread ];

  includes =
    [ ../../../include/SDL

    ];

  compileCC' = src: compileCC {
    inherit src libs externalIncludes includes;
    #extraFlags = map (f: "-I"+f) includes;
  };

in
linkSharedLibrary {
  name = "sdl";
  inherit libs;

  objects = map
    compileCC'
    [ ./video/SDL_genode_fb_video.cc
      ./video/SDL_genode_fb_events.cc
      ./audio/SDL_genodeaudio.cc
      ./timer/SDL_systimer.cc
      ./loadso/SDL_loadso.cc
    ];

  externalObjects = compileCRepo {
    sourceRoot = sdlSrc;
    inherit libs externalIncludes includes;
    sources = fromDir "${sdlSrc}/src"
      [ # main files
        "SDL.c" "SDL_error.c" "SDL_fatal.c"

        # stdlib files
        "stdlib/SDL_getenv.c"
        "stdlib/SDL_string.c"

        # thread subsystem
        "thread/SDL_thread.c"
        "thread/pthread/SDL_systhread.c"
        "thread/generic/SDL_syscond.c"
        "thread/generic/SDL_sysmutex.c"
        "thread/pthread/SDL_syssem.c"

        # cpuinfo subsystem
        "cpuinfo/SDL_cpuinfo.c"

        # timer subsystem
        "timer/SDL_timer.c"

        # video subsystem
        "video/*.c"

        # event subsystem
        "events/*.c"

        # audio subsystem
        "audio/*.c"

        # file I/O subsystem
        "file/SDL_rwops.c"

        # joystick subsystem
        "joystick/SDL_joystick.c"
        "joystick/dummy/SDL_sysjoystick.c"

        # cdrom subsystem
        "cdrom/SDL_cdrom.c"
        "cdrom/dummy/SDL_syscdrom.c"
      ];
  };

  propagate =
    { externalIncludes = [ sdlSrc.include "${sdlSrc.include}/SDL" ];
      includes = [ ../../../include ];
    };
}
