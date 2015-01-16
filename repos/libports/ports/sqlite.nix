{ preparePort, fetchurl, unzip }:

preparePort {
  name = "sqlite";
  outputs = [ "source" "include" ];
  buildInputs = [ unzip ];

  src = fetchurl {
    url = "http://www.sqlite.org/2014/sqlite-amalgamation-3080704.zip";
    sha256 = "0sfy82yv3jv3shy50qxyjaxx3jd122k84sli3jv9vdcl1qrb5by9";
  };

  extractFlags = "-j";

  preInstall = "mkdir $include; cp sqlite3.h $include";
}