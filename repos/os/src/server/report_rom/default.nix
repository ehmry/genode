{ genodeEnv, compileCC, base, server, config }:

genodeEnv.mkComponent {
  name = "report_rom";
  libs = [ base server config ];
  objects = compileCC {
    src = ./main.cc; systemIncludes = [ ../report_rom ];
  };
}
