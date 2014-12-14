{ linkComponent, compileCC, filterHeaders
, base, config, server }:

linkComponent {
  name = "ram_fs";
  libs = [ base config server ];
  objects = [(compileCC {
    src = ./main.cc;
    systemIncludes = [ (filterHeaders ../ram_fs) ];
  })];
}
