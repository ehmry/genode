{ linkComponent, compileCC, base }:

linkComponent {
  name = "terminal_log";
  libs = [ base ];
  objects = compileCC {
    src = ./main.cc;
  };
}

