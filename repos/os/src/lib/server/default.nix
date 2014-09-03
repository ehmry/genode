{ build, base, os }:

build.library {
  name = "server";
  sources = [ ./server.cc ];
  includeDirs = os.includeDirs ++ base.includeDirs;
}