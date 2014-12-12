{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-cap_integrity";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
