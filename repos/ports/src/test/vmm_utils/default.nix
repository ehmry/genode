{ spec, linkComponent, compileCC, base }:

if spec.isNOVA then linkComponent rec {
  name = "test-vmm_utils";
  libs = [ base ];
  objects = (compileCC {
    src = ./main.cc;
    inherit libs;
    includes = [ ../../../include ];
  });
} else null
