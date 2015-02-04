{ linkComponent, compileCC, base, server }:

linkComponent rec {
  name = "log_terminal";
  libs = [ base server ];
  objects = [(compileCC { src = ./main.cc; inherit libs; })];
}
