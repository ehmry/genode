{ linkComponent, compileCC, base, alarm }:

linkComponent {
  name = "test-alarm";
  objects = compileCC { src = ./main.cc; };
  libs = [ base alarm ];
}
