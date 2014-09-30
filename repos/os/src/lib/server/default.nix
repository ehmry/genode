/*
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ tool }: with tool;

# no library dependencies
{ }:

buildLibrary {
  name = "server";
  sources = [ ./server.cc ];
}