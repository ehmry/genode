{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-blk-cli";
  libs = [ base ];
  objects = [( compileCC { src = ./main.cc; } )];
}
