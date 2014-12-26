{ spec, linkComponent, compileCC, base }:

if spec.isNOVA then linkComponent {
  name = "test-vmm_utils";
  libs = [ base ];
  objects = [( compileCC { src = ./main.cc; } )];
} else null
