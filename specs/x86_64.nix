import ./x86.nix //
{ bits = 64;

  ccMarch = [ "-m64" ];
  ldMarch = [ "-melf_x86_64" "-z" "max-page-size=0x1000" ];
  asMarch = [ ];
}