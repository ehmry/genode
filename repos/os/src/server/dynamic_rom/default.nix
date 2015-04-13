{ linkComponent, compileCC, base, server, config }:

linkComponent {
  name = "dynamic_rom";
  libs = [ base server config ];
  objects = compileCC {
    src = ./main.cc;
  };
}