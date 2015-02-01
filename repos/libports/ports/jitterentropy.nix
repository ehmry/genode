{ preparePort, fetchurl }:


preparePort rec {
  name = "jitterentropy-20140411";
  outputs = [ "source" "include" ];

  src = fetchurl {
    url = "http://www.chronox.de/jent/${name}.tar.bz2";
    sha256 = "184scbjsqr5hwdi3cgkgi1rrnvbyyabz5lc2zxhf16wwxlb1p1dq";
  };

  patches = ../src/lib/jitterentropy/jitterentropy_h.patch;
  patchFlags = "-p1";

  preInstall =
    ''
      mkdir $include; mv jitterentropy.h $include
    '';
}