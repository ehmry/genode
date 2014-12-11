{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-xml_generator";
  libs = [ base ];
  objects = [ (compileCC { src = ./main.cc; }) ];
}
