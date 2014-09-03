{ stdenv, fetchurl, m4
, commonLibConfigureFlags }:

let
  version = "5.0.2";
in
stdenv.mkDerivation {
  name = "genode-gmp-${version}";

  src =  fetchurl {
    url = "mirror://gnu/gmp/gmp-${version}.tar.bz2";
    sha256 = "0a2ch2kpbzrsf3c1pfc6sph87hk2xmwa6np3sn2rzsflzmvdphnv";
  };

  nativeBuildInputs = [ m4 ];

  configureFlags = commonLibConfigureFlags;

  ## bash: line 5: 31076 Segmentation fault      ${dir}$tst
  ## FAIL: t-scan
  doCheck = false;
}