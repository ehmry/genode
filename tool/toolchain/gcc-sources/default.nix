{ stdenv, fetchurl }:

stdenv.mkDerivation rec {
  version = "4.7.2";
  name = "genode-gcc-${version}-sources";
  

  src = fetchurl {
    url = "mirror://gnu/gcc/gcc-${version}/gcc-${version}.tar.bz2";
    sha256 = "115h03hil99ljig8lkrq4qk426awmzh0g99wrrggxf8g07bq74la";
  };

  patches = import ../../patches/gcc-4.7.2;

  builder = ./builder.sh;

}