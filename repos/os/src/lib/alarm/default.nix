/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build, base, os }:

build.library {
  name = "alarm";
  shared = false;
  sources = [ ./alarm.cc ];
  includeDirs = [ os.includeDir ] ++ base.includeDirs;
}