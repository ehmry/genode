{ spec }:

import ../default.nix {
 spec = rec
   { system = "x86_${toString bits}-${spec.kernel}-genode";
     platform = "x86";
     bits = 64;
     target = "x86_64-elf";

     ccMarch = [ "-m64" ];
     ldMarch = [ "-melf_x86_64" "-z" "max-page-size=0x1000" ];
     asMarch = [ ];
   } // spec ;
}