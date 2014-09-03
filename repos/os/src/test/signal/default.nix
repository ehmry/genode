/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build, base, os }:

build.test {
  name = "test-signal";
  libs = [ base.lib.base ];
  sources = [ ./main.cc ];
  includeDirs = [ os.includeDir ] ++ base.includeDirs;
}