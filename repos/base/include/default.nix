{ build }:

let includeDir = ../include; in
[
  ( if build.isLinux then ../../../repos/base-linux/include else
    if build.isNova  then ../../../repos/base-nova/include  else
    abort "no base support for ${build.system}"
  )

  ( if build.is32Bit then includeDir + "/32bit" else
    if build.is64Bit then includeDir + "/64bit" else
    abort "platform is not known to be 32 or 64 bit"
  )

  ( if build.isx86 then includeDir + "/x86" else
    if build.isArm then includeDir + "/arm" else
    abort "unknown cpu for platform"
  )

  ( if build.isx86_32 then includeDir + "/x86_32" else
    if build.isx86_64 then includeDir + "/x86_64" else
    abort "unknown cpu for platform"
  )

  includeDir
  "${build.toolchain.glibc}/include"
]