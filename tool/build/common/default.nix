{ nixpkgs, toolchain }:

let

  shell = nixpkgs.bash + "/bin/sh";

  initialPath = (import ../common-path.nix) { pkgs = nixpkgs; };

in

derivation {
  name = "common-build-environment";
  system = builtins.currentSystem;

  builder = shell;
  args = ["-e" ./builder.sh];
 
  setup = ./setup.sh;

  inherit shell toolchain;

  initialPath = initialPath ++ [ toolchain ];

  propagatedUserEnvPkgs = [ toolchain ] ++
    nixpkgs.lib.filter nixpkgs.lib.isDerivation initialPath;
}