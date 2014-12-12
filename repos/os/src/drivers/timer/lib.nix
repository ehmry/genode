{ genodeEnv, linkStaticLibrary, compileCC, base, alarm, syscall }:

let
  arch =
    if genodeEnv.isLinux then
      rec {
        libs = [ syscall ];
        localIncludes = [ ./include ./include_periodic ];
        object = compileCC {
          inherit libs;
          src = ./linux/platform_timer.cc;
        };
      }
    else
    if genodeEnv.isNova then { localIncludes = [ ./include ./nova ]; }
    else
    throw "no timer for ${genodeEnv.system}";
in
linkStaticLibrary {
  name = "timer";
  libs = [ base alarm ] ++ arch.libs or [];
  objects =
    [ (compileCC { src = ./main.cc; inherit (arch) localIncludes; })
      (arch.object or null)
    ];
}
