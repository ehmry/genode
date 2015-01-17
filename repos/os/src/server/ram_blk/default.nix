{ linkComponent, compileCC, base, config, server }:

linkComponent {
  name = "ram_blk";
  libs = [ base config server ];
  objects = compileCC { src = ./main.cc; };
  runtime.provides = [ "Block" ];
}