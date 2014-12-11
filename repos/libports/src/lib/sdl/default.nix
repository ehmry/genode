{ genodeEnv, linkSharedLibrary, compileCC, compileCRepo
, sdlSrc, libc, pthread }:

with genodeEnv.tool;

let

  localIncludes = map
    (fn: "${sdlSrc}/${fn}")
    ( [ "include" ] ++
      (map (d: "src/"+d) [ "audio" "events" "timer" "video" ])
    );

  libs = [ libc pthread ];

  systemIncludes =
    [ ../../../include/SDL
      sdlSrc.include
      "${sdlSrc.include}/SDL"
    ];

  compileCC' = src: compileCC {
    inherit src libs systemIncludes;
    extraFlags = map (f: "-I"+f) localIncludes;
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
    inherit systemIncludes libs;
    sourceRoot = sdlSrc;
    inherit localIncludes;
    sources = genodeEnv.tool.fromDir "src"
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

  propagatedIncludes =
    [ sdlSrc.include "${sdlSrc.include}/SDL" ../../../include ];
}
