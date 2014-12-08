{ genodeEnv, linkSharedLibrary, compileCC, compileCRepo
, sdlSrc, libc, pthread }:

with genodeEnv.tool;

let

  sdlInclude = shellDerivation {
    name = "include-SDL";
    script = builtins.toFile "include-SDL.sh" ''
      out=$out/SDL
      mkdir -p $out
      cp $sdlSrc/include/*.h $out
    '';#*/
    PATH = "${nixpkgs.coreutils}/bin";
    inherit sdlSrc;
  };


  localIncludes = map
    (fn: "${sdlSrc}/${fn}")
    ( [ "include" ] ++
      (map (d: "src/"+d) [ "audio" "events" "timer" "video" ])
    );

  systemIncludes =
    [ ../../../include/SDL sdlInclude ] ++
    genodeEnv.tool.propagateIncludes [ libc ];

  compileCC' = src: compileCC {
    inherit src systemIncludes;
    extraFlags = map (f: "-I"+f) localIncludes;
  };

in
linkSharedLibrary rec {
  name = "sdl";

  libs = [ libc pthread ];

  objects = map
    compileCC'
    [ ./video/SDL_genode_fb_video.cc
      ./video/SDL_genode_fb_events.cc
      ./audio/SDL_genodeaudio.cc
      ./timer/SDL_systimer.cc
      ./loadso/SDL_loadso.cc
    ];

  externalObjects = compileCRepo {
    inherit name systemIncludes;
    sourceRoot = sdlSrc;
    inherit localIncludes;
    sources = genodeEnv.tool.fromDir "src"
      [ # main files
        "SDL.c" "SDL_error.c" "SDL_fatal.c"

        # stdlib files
        "stdlib/SDL_getenv.c"

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
        "src/video/*.c"

        # event subsystem
        "src/events/*.c"

        # audio subsystem
        "src/audio/*.c"

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

  propagatedIncludes = [ sdlInclude ../../../include ];
}
