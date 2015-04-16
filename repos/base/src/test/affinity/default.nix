{ linkComponent, compileCC, repoDir, base }:

linkComponent {
  name = "test-affinity";
  libs = [ base ];
  objects = compileCC { src = ./main.cc; };
  runtime.resources.RAM = "1M";
}
