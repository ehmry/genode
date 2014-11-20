{ genodeEnv }:

with genodeEnv.tool.nixpkgs;

let
    rev = "f304d54b176ef7b1de9bd6fff6884e1444a0c116";
    shortRev = builtins.substring 0 7 rev;
in
stdenv.mkDerivation {
  name = "nova-${shortRev}";

  src = fetchgit {
    url = https://github.com/alex-ab/NOVA.git;
    inherit rev;
    sha256 = "0rdqzjzhzx91yamrp3bw4i1lnz780mqk7p0fkvl7p4aaas6n4jkr";
  };

  sourceRoot = "git-export/build";

  makeFlags =
    if genodeEnv.is32Bit then [ "ARCH=x86_32" ] else
    if genodeEnv.is64Bit then [ "ARCH=x86_64" ] else
    throw "will not build a ${toString spec.bits} copy of NOVA";

  preBuild =
    ''
      substituteInPlace Makefile --replace '$(call gitrv)' ${shortRev}
      makeFlagsArray=(INS_DIR=$out)
    '';

  postInstall = "mv $out/hypervisor-x86_?? $out/hypervisor";
}
