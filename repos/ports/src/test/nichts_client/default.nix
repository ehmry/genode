{ linkComponent, compileCC, base, config }:

linkComponent rec {
  name = "nichts_client";
  libs = [ base config ];
  objects = compileCC {
    src = ./main.cc;
  };
}