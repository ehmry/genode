{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-loader";
  libs = [ base ];
  objects = [ (compileCC { src = ./main.cc; }) ];
}
