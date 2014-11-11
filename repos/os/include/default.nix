{ genodeEnv }:

with genodeEnv;

let includeDir = ../include; in
[ ( #if isARMv6 then includeDir + "/arm_6" else
    #if isARMv7 then includeDir + "/arm_7" else
    if isx86_32 then includeDir + "/x86_32" else
    if isx86_64 then includeDir + "/x86_64" else
    abort "no os support for ${genodeEnv.system}"
  )
  includeDir
]
