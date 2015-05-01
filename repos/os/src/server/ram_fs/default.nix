{ linkComponent, compileCC
, base, config, server }:

linkComponent {
  name = "ram_fs";
  libs = [ base config server ];
  objects = [(compileCC {
    src = ./main.cc;
    includes = [ ../ram_fs ];
  })];
  runtime.provides = [ "File_system" ];
}
