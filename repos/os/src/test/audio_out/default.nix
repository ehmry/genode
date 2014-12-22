{ linkComponent, compileCC, base, config }:

linkComponent {
  name = "test-audio_out";
  libs = [ base config ];
  objects = [ (compileCC { src = ./main.cc; }) ];
}
