{ linkComponent, compileCC, base }:

linkComponent {
  name = "testnit";
  libs = [ base ];
  objects = compileCC { src = ./test.cc; };
}
