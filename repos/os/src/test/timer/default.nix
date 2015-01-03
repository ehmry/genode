{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-timer";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
  runtime.requires = [ "Timer" ];
}
