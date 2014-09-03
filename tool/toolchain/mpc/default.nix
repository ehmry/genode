{ stdenv, fetchurl
, gmp, mpfr
, commonLibConfigureFlags }:

let
  version = "0.9";
in
stdenv.mkDerivation rec {
  name = "genode-mpc-${version}";

  src = fetchurl {
    url = "http://www.multiprecision.org/mpc/download/mpc-${version}.tar.gz";
    sha256 = "1b29n3gd9awla1645nmyy8dkhbhs1p0g504y0n94ai8d5x1gwgpx";
  };

  buildInputs = [ gmp mpfr ];

  configureFlags = commonLibConfigureFlags;

  doCheck = true;
}