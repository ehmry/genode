/*
 * \brief  Function to execute a NOVA run
 * \author Emery Hemingway
 * \date   2014-08-16
 */

 { spec, nixpkgs, tool }:

let iso = import ../iso { inherit nixpkgs tool; }; in

{ name, bootInputs
, waitRegex ? "",  waitTimeout ? 0
, graphical ? false, qemuArgs ? "", ... } @ args:

let
  hypervisor = nixpkgs.stdenv.mkDerivation rec {
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
      if spec.bits == 32 then [ "ARCH=x86_32" ] else
      if spec.bits == 64 then [ "ARCH=x86_64" ] else
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

in
derivation (args // {
  name = name+"-run";
  system = builtins.currentSystem;
  preferLocalBuild = true;

  PATH =
    "${nixpkgs.coreutils}/bin:" +
    "${nixpkgs.findutils}/bin";

  builder = nixpkgs.expect + "/bin/expect";
  args = 
    [ "-nN"
      ../../../../tool/run-nix-setup.exp
      # setup.exp will source the files that follow
      ../../../../tool/run
      ./nova.exp
    ];

  inherit waitRegex waitTimeout graphical qemuArgs;

  # Only the x86_64 variant of Qemu provides the emulation of hardware
  # virtualization features used by NOVA. So let's always stick to this
  # variant of Qemu when working with NOVA even when operating in 32bit.
  qemu = nixpkgs.qemu + "/bin/qemu-system-x86_64";

  diskImage = iso {
    inherit name hypervisor;
    genodeImage = tool.bootImage {
      inherit name;
      inputs = bootInputs;
    };
  };

})