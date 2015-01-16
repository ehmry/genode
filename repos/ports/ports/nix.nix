{ preparePort, fetchurl }:

preparePort rec {
  name = "nix-1.8";

  src = fetchurl {
    url = "http://nixos.org/releases/nix/${name}/${name}.tar.xz";
    sha256 = "077hircacgi9y4n6kf48qp4laz1h3ab6sif3rcci1jy13f05w2m3";
  };

  patches = ../src/app/nix/patches/genode.patch;
  patchFlags = "-p1";
}