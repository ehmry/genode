{ linkComponent, compileCC, base }:

linkComponent {
  name = "pointer";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
