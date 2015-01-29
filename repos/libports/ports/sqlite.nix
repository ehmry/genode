{ preparePort, fetchurl, unzip }:

preparePort {
  name = "sqlite-3.8.8.2";
  outputs = [ "source" "include" ];
  buildInputs = [ unzip ];

  src = fetchurl {
    url = "http://sqlite.org/2015/sqlite-amalgamation-3080802.zip";
    sha256 = "0qvnwmqykhk1dxgynqgync6mb8d7naclgy3z1vm91l3mzc6ymgjy";
  };

  extractFlags = "-j";

  preInstall = "mkdir $include; cp sqlite3.h $include";
}