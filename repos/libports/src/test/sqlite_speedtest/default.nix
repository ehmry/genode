{ linkComponent, compileC, compileCRepo, sqlite_rawSrc, libc, sqlite }:

linkComponent rec {
  name = "test-sqlite_speedtest";
  libs = [ libc sqlite ];

  externalObjects = compileCRepo {
    inherit name libs;
    sourceRoot = sqlite_rawSrc;
    sources = "test/speedtest1.c";
  };
}