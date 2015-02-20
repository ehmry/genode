{ preparePort, fetchurl }:


preparePort rec {
  name = "jitterentropy-1.1.0";
  outputs = [ "source" "include" ];

  src = fetchurl {
    url = "http://www.chronox.de/jent/${name}.tar.xz";
    sha256 = "13i62gnpcklrprpycfxim3j0gslsv8rv2wi3by472gsfm4i3axxm";
  };

  patches = ../src/lib/jitterentropy/jitterentropy_h.patch;
  patchFlags = "-p1";

  preInstall =
    ''
      mkdir $include; mv jitterentropy.h $include
    '';
}
