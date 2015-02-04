{ linkComponent, compileCC, config, scout_widgets }:

let
  compileCC' = src: compileCC {
    inherit src; includes = [ ../../app/scout ];
  };
in
linkComponent {
  name = "liquid_fb";
  libs = [ config scout_widgets ];
  objects = map compileCC' [ ./main.cc ./services.cc ];
}
