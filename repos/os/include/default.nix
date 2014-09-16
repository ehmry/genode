{ build }:

let includeDir = ../include; in
[ ( #if build.isARMv6 then includeDir + "/arm_6" else
    #if build.isARMv7 then includeDir + "/arm_7" else
    if build.isx86_32 then includeDir + "/x86_32" else
    if build.isx86_64 then includeDir + "/x86_64" else
    abort "no os support for build.system"
  )
  includeDir
]