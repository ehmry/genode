{ genodeEnv }:

with genodeEnv;

let
  repoDir =
     if isLinux then ../../base-linux else
     if isNOVA  then ../../base-nova  else
     abort "no base support for ${system}";
  baseIncDir = ../include;

  platform =
    if isx86_32 then "x86_32" else
    if isx86_64 then "x86_64" else
    abort "unknown cpu for platform";
in

import (repoDir+"/include") { inherit genodeEnv; }
++
[
  (../src/platform + "/${platform}")
  (repoDir+"/src/platform")

  ( if isx86 then baseIncDir+"/x86" else
    if isArm then baseIncDir+"/arm" else
    abort "unknown cpu for platform"
  )

  (baseIncDir+"/${platform}")

  #( if isx86_32 then ../../os/include/x86_32 else
  #  if isx86_64 then ../../os/include/x86_64 else
  #  abort "unknown cpu for platform"
  #)

  ( if is32Bit then baseIncDir+"/32bit" else
    if is64Bit then baseIncDir+"/64bit" else
    abort "platform is not known to be 32 or 64 bit"
  )
  
  baseIncDir
]
