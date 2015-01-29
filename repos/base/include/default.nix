{ spec, filterHeaders }:

with spec;

let
  repoDir =
     if isFiasco then ../../base-fiasco else
     if isLinux  then ../../base-linux  else
     if isNOVA   then ../../base-nova   else
     abort "base includes not expressed for ${system}";
  baseIncDir = ../include;

  platform =
    if isx86_32 then "x86_32" else
    if isx86_64 then "x86_64" else
    abort "unknown cpu for platform";
in
map filterHeaders [
  ( if isx86 then baseIncDir+"/x86" else
    if isArm then baseIncDir+"/arm" else
    abort "unknown cpu for platform"
  )

  (baseIncDir+"/${platform}")

  ( if isx86_32 then ../../os/include/x86_32 else
    if isx86_64 then ../../os/include/x86_64 else
    abort "unknown cpu for platform"
  )
]
++
import (repoDir+"/include") { inherit spec; }
++
[
  ( if is32Bit then baseIncDir+"/32bit" else
    if is64Bit then baseIncDir+"/64bit" else
    abort "platform is not known to be 32 or 64 bit"
  )

  baseIncDir
] ++
import ../../os/include { inherit spec filterHeaders; }
