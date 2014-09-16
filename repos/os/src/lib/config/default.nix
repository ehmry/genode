/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build }:

# no library dependencies
{ }:

build.library {
  name = "config";
  shared = false;
  sources = [ ./config.cc ];
}