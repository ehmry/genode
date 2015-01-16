{ linkStaticLibrary, compileCRepo, sqliteSrc, libc }:

linkStaticLibrary rec {
  name = "sqlite";
  libs = [ libc ];
  externalObjects = compileCRepo {
    inherit libs;
    sourceRoot = sqliteSrc;
    sources = "sqlite3.c";
  };
  propagate.systemIncludes = [ sqliteSrc.include ];
}
