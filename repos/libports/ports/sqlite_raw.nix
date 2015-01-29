{ preparePort, fetchurl, unzip }:

preparePort {
  name = "sqlite-raw-3.8.8.2";
  buildInputs = [ unzip ];

  src = fetchurl {
    url = "http://www.sqlite.org/2015/sqlite-src-3080802.zip";
    sha256 = "1h8fsc92qw2r9k0issbwa6yrvj7n069drxc4j181gfh1fzxlvx3w";
  };

  extractFlags = "-j";
}
