/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build, base, os }:

build.test {
  name = "bomb";
  libs = [ base.lib.base os.lib.config ];
  sources = [ ./main.cc ];
  includeDirs = [ os.includeDir ] ++ base.includeDirs;
}