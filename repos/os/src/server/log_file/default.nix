{ linkComponent, compileCC, server, config, base }:

linkComponent {
  name = "log_file";
  libs = [ server config base ];
  objects = compileCC {
    src = ./main.cc;
  };
  runtime =
    { provides = [ "LOG" ];
      requires = [ "File_system" ];
    };
}