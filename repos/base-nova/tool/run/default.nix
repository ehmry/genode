/*
 * \brief  Function to execute a NOVA run
 * \author Emery Hemingway
 * \date   2014-08-16
 */

 { tool, pkgs }:

with tool;

let iso = import ../iso { inherit tool; }; in

{ name, contents, testScript }:

let
  hypervisor = tool.nixpkgs.stdenv.mkDerivation rec {
    rev = "f304d54b176ef7b1de9bd6fff6884e1444a0c116";
    shortRev = builtins.substring 0 7 rev;
    name = "nova-${shortRev}";
    src = nixpkgs.fetchgit {
      url = https://github.com/alex-ab/NOVA.git;
      inherit rev;
      sha256 = "0rdqzjzhzx91yamrp3bw4i1lnz780mqk7p0fkvl7p4aaas6n4jkr";
    };
    sourceRoot = "git-export/build";
    makeFlags =
      if pkgs.system == "x86_32-nova" then [ "ARCH=x86_32" ] else
      if pkgs.system == "x86_64-nova" then [ "ARCH=x86_64" ] else
      throw "will not build a ${toString spec.bits} copy of NOVA";
    preBuild = ''
      substituteInPlace Makefile --replace '$(call gitrv)' ${shortRev}
      makeFlagsArray=(INS_DIR=$out)
    '';
  };

  pulsarBz2 = nixpkgs.fetchurl {
    url = http://hypervisor.org/pulsar/pulsar-0.5.bz2;
    sha256 = "02dlr26xajbdvg2zhs5g9k57kd68d31vqqfpgc56k4ci3kwhbrkw";
  };

  contents' =
    ( map ({ target, source }:
        { target = "/genode/${target}"; inherit source; })
        contents
    ) ++ [
      { target = "/genode";
        source = pkgs.core;
      }
      { target = "/genode";
        source = pkgs.init;
      }
      #{ target = "/genode";
      #  source = pkgs.libs.ld;
      #}
      { target = "/boot";
        source = hypervisor;
      }
    ];

in
derivation {
  name = name+"-run";
  system = builtins.currentSystem;
  preferLocalBuild = true;

  PATH =
    "${nixpkgs.coreutils}/bin:" +
    "${nixpkgs.findutils}/bin:" +
    "${nixpkgs.qemu}/bin";

  builder = nixpkgs.expect + "/bin/expect";
  args = 
    [ "-nN"
      ../../../../tool/run-nix-setup.exp
      # setup.exp will source the files that follow
      ../../../../tool/run
      ./nova.exp
    ];

  inherit testScript;

  diskImage = iso { inherit name; contents = contents'; };
}
