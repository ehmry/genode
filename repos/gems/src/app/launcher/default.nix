{ linkComponent, compileCC, base, server, config }:

linkComponent {
  name = "launcher";
  libs = [ base server config ];
  objects = compileCC {
    src = ./main.cc;
    includes = [ ../launcher ];
  };
}