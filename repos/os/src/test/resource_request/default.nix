{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-resource_request";
  libs = [ base ];
  objects = compileCC {
    src = ./main.cc;
  };
}
