{ preparePort, fetchurl }:

let version = "1.0.6"; in
preparePort {
  name = "bzip2";
  outputs = [ "source" "include" ];

  src = fetchurl {
    url = "http://www.bzip.org/${version}/bzip2-${version}.tar.gz";
    sha256 = "1kfrc7f0ja9fdn6j1y6yir6li818npy6217hvr3wzmnmzhs8z152";
  };

  preInstall = "mkdir $include; cp bzlib.h $include";
}
