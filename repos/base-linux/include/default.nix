{ genodeEnv }:

with genodeEnv;

[ #( if is32Bit then ../src/platform/x86_32 else
  #  if is64Bit then ../src/platform/x86_64 else
  #  abort "no base support for ${system}"
  #)
  ../include
]
