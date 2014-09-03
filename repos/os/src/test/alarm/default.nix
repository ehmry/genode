/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build, base, os }:

build.test {
  name = "test-alarm";
  sources = [ ./main.cc ];
  libs = [ base.lib.base os.lib.alarm ];
  includeDirs = [ os.includeDir ] ++ base.includeDirs;
}