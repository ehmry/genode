{ nixpkgs }:
let
  shell = nixpkgs.bash + "/bin/sh";

   initialPath =
    [ nixpkgs.coreutils
      nixpkgs.findutils
      nixpkgs.diffutils
      nixpkgs.gawk
      nixpkgs.gnugrep
      nixpkgs.gnused
      nixpkgs.gnutar
      nixpkgs.patch
      nixpkgs.gzip
      nixpkgs.bzip2
      nixpkgs.xz
    ];

  result = derivation {
    name = "prepare-port";
    system = builtins.currentSystem;
    builder = shell;
    args = [ "-e" ./builder.sh ];
    setup = ./setup.sh;
    inherit initialPath;
  };
in
{ name
, outputs ? [ "source" ]
, ... } @ attrs:
derivation (attrs // {
  inherit name outputs;
  preparePort = result;
  system = builtins.currentSystem;

  builder = attrs.realBuilder or shell;
  args = attrs.args or [ "-e" (attrs.builder or ./default-builder.sh) ];
})
