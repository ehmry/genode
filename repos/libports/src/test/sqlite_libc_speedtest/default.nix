{ linkComponent, compileC, compileCRepo, nixpkgs, libc, libc_sqlite }:

linkComponent rec {
  name = "test-sqlite_libc_speedtest";
  libs = [ libc libc_sqlite ];

  externalObjects = compileCRepo {
    inherit name libs;
    sources = nixpkgs.fetchurl {
      name = "speedtest1.c";
      url = http://www.sqlite.org/cgi/src/raw/test/speedtest1.c?name=e4e2aa37ff66bad9f414a50a8cb9edfaac65c9e5;
      sha256 = "0g2n8jywvvbhpndbhf1n20xl9wls5qsyfrpypfxl1w07m21glf3f";
    };
  };
}
