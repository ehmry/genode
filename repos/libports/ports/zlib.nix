/*
 * \author Emery Hemingway
 * \date   2014-09-30
 */

{ preparePort, fetchurl }:
let
  version = "1.2.8";
  # LICENSE   := zlib
in
preparePort {
  name = "zlib-${version}";
  src = fetchurl {
    url = "mirror://sourceforge/libpng/zlib/${version}/zlib-${version}.tar.gz";
    sha256 = "039agw5rqvqny92cpkrfn243x2gd4xn13hs3xi6isk55d2vqqr9n";
  };
}