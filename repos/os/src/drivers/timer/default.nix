{ linkComponent, compileCC, timer }:

linkComponent {
  name = "timer";
  libs = [ timer ];
  objects = compileCC { src = ./empty.cc; };
}
