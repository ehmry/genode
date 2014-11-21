{ genodeEnv, compileCC, base, alarm, syscall }:

let
  arch =
    if genodeEnv.isLinux then
      rec { libs = [ syscall ];
        localIncludes = [ ./include ./include_periodic ];
        object = compileCC {
          src = ./linux/platform_timer.cc;
          inherit localIncludes;
          systemIncludes = [ syscall.include ];
        };
      }
    else
    if genodeEnv.isNova then { localIncludes = [ ./include ./nova ]; }
    else
    throw "no timer for ${genodeEnv.system}";
in
genodeEnv.mkLibrary {
  name = "timer";
  libs = [ base alarm ] ++ arch.libs or [];
  objects =
    [ (compileCC { src = ./main.cc; inherit (arch) localIncludes; })
      (arch.object or null)
    ];  
}
