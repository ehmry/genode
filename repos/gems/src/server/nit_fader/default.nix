{ linkComponent, compileCC, base, server, config, blit }:

linkComponent {
  name = "nit_fader";
  libs = [ base server config blit ];
  objects = compileCC {
    src = ./main.cc;
    includes = [ ../nit_fader ../../../../demo/include ];
  };
}
