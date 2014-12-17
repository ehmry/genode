{ system }:

let
  spec =
    if system == "i686-linux"   then import ./i686-linux.nix else
    if system == "x86_64-linux" then import ./x86_64-linux.nix else
    if system == "i686-nova"    then import ./i686-nova.nix  else
    if system == "x86_64-nova"  then import ./x86_64-nova.nix  else
    abort "unknown system type ${system}";
in
spec // rec {
  inherit system;
  is32Bit  = spec.bits == 32;
  is64Bit  = spec.bits == 64;
  isArm = spec.platform == "arm";
  isx86 = spec.platform == "x86";
  isi686 = isx86 && is32Bit;
  isx86_32 = isi686;
  isx86_64 = isx86 && is64Bit;

  isLinux = spec.kernel == "linux";
  isNova  = spec.kernel == "nova";
  isNOVA  = isNova;
}
