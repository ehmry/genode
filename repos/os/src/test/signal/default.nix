{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-signal";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
