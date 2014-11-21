{ genodeEnv, compileCC, base }:

if genodeEnv.isx86 then genodeEnv.mkComponent {
  name = "ps2_drv";
  libs = [ base ];
  objects = compileCC {
    src = ./x86/main.cc;
    localIncludes = [ ../ps2 ];
  };
} else null
