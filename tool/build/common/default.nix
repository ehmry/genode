{ spec, toolchain, nixpkgs }:

let

  shell = nixpkgs.bash + "/bin/sh";

  initialPath = (import ../common-path.nix) { pkgs = nixpkgs; };

in

derivation {
  system = builtins.currentSystem;

  name = "${spec.system}-buildenv";

  builder = shell;
  args = ["-e" ./builder.sh];
 
  setup = ./setup.sh;

  inherit shell toolchain;

  initialPath = initialPath ++ [ toolchain ];
  #  (if targetSystem == "arm-genode" then toolchain + "/arm-elf-eabi" else
  #  if targetSystem == "x86_64-genode" then toolchain + "/x86_64-elf" else
  #  throw "unknown target system \"${targetSystem}\"") ];     

  #inherit (cross.toolchain) target;

  propagatedUserEnvPkgs = [ toolchain ] ++
    nixpkgs.lib.filter nixpkgs.lib.isDerivation initialPath;
}