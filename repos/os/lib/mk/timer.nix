{ genodeEnv, base, alarm, syscall }:

genodeEnv.mkLibrary (genodeEnv.tool.mergeSets [{
  name = "timer";
  libs = [ base alarm ];
  sources = genodeEnv.fromPath ../../src/drivers/timer/main.cc;
  localIncludes = [ ../../src/drivers/timer/include ];
}
( if genodeEnv.isLinux then
      import ./linux/timer.nix { inherit genodeEnv syscall; }
  else
  if genodeEnv.isNOVA then
      import ./nova/timer.nix { inherit genodeEnv; }
  else
  throw "no libs.timer for ${genodeEnv.system}"
)
])
