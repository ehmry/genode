/*
 * \author Emery Hemingway
 * \date   2014-09-30
 */

{ preparePort, fetchurl }:
let
  name = "libpng-1.4.1";
  # LICENSE   := PNG
in
preparePort {
  inherit name;
  src = fetchurl {
    url = "mirror://sourceforge/libpng/${name}.tar.gz";
    sha256 = "05r41pdbgj73vj28l3si8cnv0hsvsfpayjp4n792d9h5b04y298c";
  };
}