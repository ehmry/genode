/*
 * \brief  
 * \author Emery Hemingway
 * \date   2014-08-10
 */

{ build, base, os }:

build.library {
  name = "config_args";
  shared = false;
  sources = [ ./config_args.cc ];
  libs = [ os.lib.config ];
  includeDirs = [ os.includeDir ] ++ base.includeDirs;
}