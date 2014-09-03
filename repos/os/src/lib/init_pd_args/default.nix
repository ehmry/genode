/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build, base, os }:

build.library {
  name = "init_pd_args";
  shared = false;
  sources = [ ../../init/pd_args.cc ];
  includeDirs = [ os.includeDir ] ++ base.includeDirs;
}