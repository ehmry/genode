let
  spec = import ./x86_32.nix // import ./nova.nix;
in
import ../default.nix {
  spec =
    spec // rec {
      system = "x86_${toString spec.bits}-${spec.kernel}-genode";
   };
}