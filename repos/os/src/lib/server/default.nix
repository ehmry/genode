/*
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ build }:

# no library dependencies
{ }:

build.library {
  name = "server";
  sources = [ ./server.cc ];
}