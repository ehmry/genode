{ spec, compiler, common, nixpkgs }:

let

  shell = nixpkgs.bash + "/bin/sh";

  result = derivation
    { system = builtins.currentSystem;
      name = "${spec.system}-link-shared";

      inherit common;
      script = ./link.sh;

      builder = shell;
      args = [ "-e" ./builder.sh ];
    };

in

{ name, flags, objects }:
derivation {
  inherit name flags;
  system = result.system;

  inherit compiler ar ld;

  builder = shell;
  args = [ "-e" "${result}/link-shared.sh" ];
}