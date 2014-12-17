{ genodeEnv, linkComponent, compileCC, base }:

if !genodeEnv.isx86 then null else
linkComponent {
  name = "test-pci";
  libs = [ base ];
  objects = compileCC { src = ./test.cc; };
}
