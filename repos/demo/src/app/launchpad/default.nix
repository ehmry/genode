{ linkComponent, compileCC, launchpad, scout_widgets, config }:
let
  compileCC' = src: compileCC {
    inherit src;
    includes = [ ../scout ];
  };
in
linkComponent {
  name = "launchpad";
  libs = [ launchpad scout_widgets config ];
  objects = map compileCC'
    [ ./launchpad_window.cc ./launcher.cc ./main.cc ];
}
