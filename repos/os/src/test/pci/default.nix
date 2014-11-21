{ genodeEnv, compileCC, base }:

if !genodeEnv.isx86 then null else
genodeEnv.mkComponent {
  name = "test-pci";
  libs = [ base ];
  objects = compileCC { src = ./test.cc; };
}
