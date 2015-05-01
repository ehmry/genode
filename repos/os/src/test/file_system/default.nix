{ linkComponent, compileCC, base, config, blake2s }:

linkComponent {
  name = "test-file_system";
  libs = [ base config blake2s ];
  objects = compileCC { src = ./main.cc; };
  runtime.requires = [ "File_system" ];
}