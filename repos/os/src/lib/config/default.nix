/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build, base, os }:

build.library {
  name = "config";
  shared = false;
  sources = [ ./config.cc ];
  includeDirs = [ os.includeDir ] ++ base.includeDirs;
}