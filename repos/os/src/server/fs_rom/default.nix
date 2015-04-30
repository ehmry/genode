{ linkComponent, compileCC, base }:

linkComponent {
  name = "fs_rom";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
  runtime.provides = [ "ROM" ];
}
