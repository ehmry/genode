{ linkComponent, compileCC, base, config, server }:

linkComponent {
  name = "nit_fb";
  libs = [ base config server ];
  objects = compileCC {
    src = ./main.cc;
  };
}
