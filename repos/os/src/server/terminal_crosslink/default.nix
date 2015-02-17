{ linkComponent, compileCC, base }:

linkComponent {
  name = "terminal_crosslink";
  libs = [ base ];
  objects = map
    (src: compileCC { inherit src; })
    [ ./main.cc ./terminal_session_component.cc ];
}
