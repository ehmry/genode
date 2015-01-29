{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-rtc";
  libs = [ base ];
  objects = compileCC {
    src = ./main.cc;
  };
  runtime.requires = [ "Rtc" ];
}