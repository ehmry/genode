import ./x86.nix //
{ bits = 32;

  ccMarch = [ "-march=i686" "-m32" ];
  ldMarch = [ "-melf_i386" ];
  asMarch = [ "-march=i686" "--32" ];
}