/*
* \author Emery Hemingway
* \date   2014-08-12
*/

{ build, base, os }:

build.test {
  name = "test-timer";
  sources = [ ./main.cc ];
  libs = [ base.lib.base ];
  includeDirs = [ os.includeDir ] ++ base.includeDirs;
}