{ genodeEnv, linkComponent, compileCC, base, config }:

linkComponent {
  name = "tar_fs";
  libs = [ base config ];
  objects = compileCC {
    src = ./main.cc; includes = [ ../tar_fs ];
  };
}
