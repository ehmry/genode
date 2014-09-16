/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build }:

# no library dependencies
{ }:

build.library {
  name = "init_pd_args";
  shared = false;
  sources = [ ../../init/pd_args.cc ];
}