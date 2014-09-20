{ tool }:

with tool.build;

let includeDir = ../include; in
[
  ( if isLinux then ../../../repos/base-linux/include else
    if isNova  then ../../../repos/base-nova/include  else
    abort "no base support for ${system}"
  )

  ( if is32Bit then includeDir + "/32bit" else
    if is64Bit then includeDir + "/64bit" else
    abort "platform is not known to be 32 or 64 bit"
  )

  ( if isx86 then includeDir + "/x86" else
    if isArm then includeDir + "/arm" else
    abort "unknown cpu for platform"
  )

  ( if isx86_32 then includeDir + "/x86_32" else
    if isx86_64 then includeDir + "/x86_64" else
    abort "unknown cpu for platform"
  )

  includeDir
  "${toolchain.glibc}/include"
]