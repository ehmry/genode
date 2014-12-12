{ linkComponent, compileCC, base, config }:

linkComponent {
  name = "tar_rom";
  libs = [ base config ];
  objects = [ (compileCC { src = ./main.cc; }) ];
}
