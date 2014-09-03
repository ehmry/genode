{ stdenv, fetchurl
, gmp
, commonLibConfigureFlags }:

let
  version = "3.1.0";
in
stdenv.mkDerivation {
  name = "genode-mpfr-${version}";

  src = fetchurl {
    url = "mirror://gnu/mpfr/mpfr-${version}.tar.bz2";
    sha256 = "105nx8qqx5x8f4rlplr2wk4cyv61iw5j3jgi2k21rpb8s6xbp9vl";
  };

  buildInputs = [ gmp ];

  configureFlags = commonLibConfigureFlags;

  doCheck = true;
}