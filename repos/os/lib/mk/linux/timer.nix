{ genodeEnv, syscall }:
{
  libs = [ syscall ];
  sources = genodeEnv.fromPath
     ../../../src/drivers/timer/linux/platform_timer.cc;
  localIncludes = [ ../../../src/drivers/timer/include_periodic ];
}
