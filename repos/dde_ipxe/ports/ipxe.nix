{ preparePort, fetchgit }:

let
  rev = "c4bce43c3c4d3c5ebb2d926b58ad16dc9642c19d";
in
preparePort {
  name = "ipxe-${builtins.substring 0 7 rev}";

  src = fetchgit {
    url = git://git.ipxe.org/ipxe.git;
    sha256 = "0qb8hc8244xk742fjpb8sfb58f39yhk78smm1mvpl8gd76ab9bfp";
    inherit rev;
  };

  patches =
    [ ../patches/dde_ipxe.patch ../patches/add_devices.patch ];

  installPhase = "cp -r src $source";
}
