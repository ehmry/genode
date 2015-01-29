{ spec, nixpkgs }:

with nixpkgs;

stdenv.mkDerivation rec {
  name = "fiasco-2014092821";

  buildInputs = [ perl ];

  src = fetchurl {
    url = "http://os.inf.tu-dresden.de/download/snapshots-oc/${name}.tar.xz";
    sha256 = "1j5cfv09zvk0prxws4v9vavij263py5xzjfci25sg1d02jrmz4z7";
  };

  sourceRoot = name + "/src/kernel/fiasco";

  inherit perl;
  postPatch =
    ''
      sed -i "s|/usr/bin/perl|$perl/bin/perl|g" \
          tool/gen_kconfig tool/preprocess/src/preprocess

      sed -i 's|/bin/pwd|pwd|g' \
          tool/kconfig/Makefile
    '';

  preBuild =
    ''
      make BUILDDIR=build
      cd build
    '';

  installPhase = "mkdir $out; cp fiasco $out";
}