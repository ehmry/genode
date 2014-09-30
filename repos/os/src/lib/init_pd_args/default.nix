/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ tool }: with tool;

# no library dependencies
{ }:

buildLibrary {
  name = "init_pd_args";
  shared = false;
  sources = [ ../../init/pd_args.cc ];
}