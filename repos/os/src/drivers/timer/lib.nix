{ spec, linkStaticLibrary, compileCC, base, alarm, syscall }:

let
  linux = rec
    { libs = [ syscall ];
      localIncludes = [ ./include_periodic ];
      objects =
        [ (compileCC {
            inherit libs localIncludes;
            src = ./linux/platform_timer.cc;
          })
        ];
    };

  nova.localIncludes = [ ./nova ];

  arch =
    if spec.isLinux then linux else
    if spec.isNOVA  then nova  else
    throw "no timer build expression for ${spec.system}";
in
linkStaticLibrary {
  name = "timer";
  libs = [ base alarm ] ++ arch.libs or [];
  objects =
    [ (compileCC {
        src = ./main.cc;
        localIncludes = [ ./include ] ++ arch.localIncludes or [];
      })
    ] ++ arch.objects or [];
}
