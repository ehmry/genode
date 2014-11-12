{ genodeEnv, base }:

if genodeEnv.isx86 then genodeEnv.mkComponent {
  name = "ps2_drv";
  libs = [ base ];
  sources = genodeEnv.fromPath ./x86/main.cc;
  localIncludes = [ ../ps2 ];
} else null
