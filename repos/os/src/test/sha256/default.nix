{ linkComponent, compileCC, base, sha256 }:

linkComponent {
  name = "test-sha256";
  libs = [ base sha256 ];
  objects = compileCC {
    src = ./main.cc;
  };
}