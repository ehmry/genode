{ spec, nixpkgs }:

with nixpkgs;

let
    rev = "2b4f2803218cf92e2982b47a370d60b18bb78a15";
    shortRev = builtins.substring 0 7 rev;
in
stdenv.mkDerivation rec {
  name = "nova-${shortRev}";

  src = fetchFromGitHub {
    owner = "alex-ab";
    repo = "NOVA";
    inherit rev;
    sha256 = "0fdvlyq2nnzq4wpgf6gkyc3nyir50z52lvf663akn7f712kjqr1j";
  };

  sourceRoot = "${src.name}/build";

  makeFlags =
    if spec.is32Bit then [ "ARCH=x86_32" ] else
    if spec.is64Bit then [ "ARCH=x86_64" ] else
    throw "will not build a ${toString spec.bits}bit copy of NOVA";

  preBuild =
    ''
      substituteInPlace Makefile --replace '$(call gitrv)' ${shortRev}
      makeFlagsArray=(INS_DIR=$out)
    '';

  passthru.args = [ "iommu" "serial" ];

  postInstall = "mv $out/hypervisor-x86_?? $out/hypervisor";
}
