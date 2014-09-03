let
  spec = import ./x86_64.nix // import ./linux.nix;
in
import ../default.nix { 
 spec = 
   spec // rec {
     system = "x86_${toString spec.bits}-${spec.kernel}-genode";
   };
}