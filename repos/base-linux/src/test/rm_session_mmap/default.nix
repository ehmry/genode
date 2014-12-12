{ linkComponent, compileCC, base }:

linkComponent {
  name = "test-rm_session_mmap";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
}
