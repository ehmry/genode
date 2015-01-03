{ spec, linkStaticLibrary, compileCC, base, alarm, syscall }:

let
  linux = rec
    { libs = [ syscall ];
      sources = [ ./linux/platform_timer.cc ];
      compile = src: compileCC {
        inherit src libs;
        localIncludes = [ ./include ./include_periodic ];
      };
    };

  nova.compile = src: compileCC {
    inherit src;
    localIncludes = [ ./include ./nova ];
  };

  arch =
    if spec.isLinux then linux else
    if spec.isNOVA  then nova  else
    throw "no timer build expression for ${spec.system}";
in
linkStaticLibrary {
  name = "timer";
  libs = [ base alarm ] ++ arch.libs or [];
  objects = map arch.compile ([ ./main.cc ] ++ arch.sources or []);

  propagate.runtime.provides = [ "Timer" ];
}
