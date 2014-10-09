{ nixpkgs, hypervisor, core, init }:
let
  shell = nixpkgs.bash + "/bin/sh";
in
derivation {
  name = "grub-boot-image";
  system = builtins.currentSystem;

  builder = shell;
  args = [ "-e" ./builder.sh ];
  PATH = nixpkgs.coreutils;

  inherit core init;
  menuEntry = ./menuentry;
}