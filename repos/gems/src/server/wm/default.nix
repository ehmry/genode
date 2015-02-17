{ linkComponent, compileCC, base, server, config }:

linkComponent {
  name=  "wm";
  libs = [ base server config ];
  objects = compileCC {
    src = ./main.cc;
    includes = [ ../wm ];
  };
}
