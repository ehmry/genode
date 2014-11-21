{ genodeEnv, compileCC, config, scout_widgets }:

let
  compileCC' = src: compileCC {
    inherit src; systemIncludes = [ ../../app/scout ];
  };
in
genodeEnv.mkComponent {
  name = "liquid_fb";
  libs = [ config scout_widgets ];
  objects = map compileCC' [ ./main.cc ./services.cc ];
}
