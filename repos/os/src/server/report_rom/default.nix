{ linkComponent, compileCC, base, server, config }:

linkComponent {
  name = "report_rom";
  libs = [ base server config ];
  objects = compileCC {
    src = ./main.cc; includes = [ ../report_rom ];
  };
}
