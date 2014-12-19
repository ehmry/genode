{ linkComponent, compileCC, base, server }:

linkComponent {
  name = "log_terminal";
  libs = [ base server ];
  objects = [(compileCC { src = ./main.cc; })];
}
