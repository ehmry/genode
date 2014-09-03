{ tool, base }:

let
  inherit (tool) build;

  repo = rec {
    sourceDir  = ./src;
    includeDir = ./include;
    includeDirs =
      [ includeDir
      ] ++ (
        if build.is32Bit then [ "${includeDir}/32bit" ] else
        if build.is64Bit then [ "${includeDir}/64bit" ] else
        abort "nova does not support ${toString build.spec.bits} bit hardware"
      );
  };

in
repo // {
  lib = {

    base = import ./lib/mk/base.nix { inherit base repo; };

    base-common = import ./lib/mk/base-common.nix { inherit build base repo; };

    syscall = { includeDirs = []; };

  };

  core = import ./src/core { inherit base repo; };

    test = {
      cap_integrity = import ./src/test/cap_integrity { inherit build base; };
    };

  final = {

    hypervisor = tool.nixpkgs.stdenv.mkDerivation rec {
      rev = "f304d54b176ef7b1de9bd6fff6884e1444a0c116";
      shortRev = builtins.substring 0 7 rev;
      name = "nova-${shortRev}";
      src = tool.nixpkgs.fetchgit {
        url = https://github.com/alex-ab/NOVA.git;
        inherit rev;
        sha256 = "0rdqzjzhzx91yamrp3bw4i1lnz780mqk7p0fkvl7p4aaas6n4jkr";
      };
      sourceRoot = "git-export/build";
      makeFlags =
        if tool.build.spec.bits == 32 then [ "ARCH=x86_32" ] else
        if tool.build.spec.bits == 64 then [ "ARCH=x86_64" ] else
        throw "will not build a ${toString tool.build.spec.bits} copy of NOVA";
      preBuild = ''
        substituteInPlace Makefile --replace '$(call gitrv)' ${shortRev}
        makeFlagsArray=(INS_DIR=$out)
      '';
    };

    pulsar = tool.nixpkgs.stdenv.mkDerivation rec {
      name = "pulsar-0.5";
      src = tool.nixpkgs.fetchurl {
        url = "http://hypervisor.org/pulsar/${name}.bz2";
        sha256 = "02dlr26xajbdvg2zhs5g9k57kd68d31vqqfpgc56k4ci3kwhbrkw";
      };
      buildCommand = "bzcat $src > $out";
    };

  };
}