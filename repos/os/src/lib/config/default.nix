/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ tool }: with tool;

# no library dependencies
{ }:

buildLibrary {
  name = "config";
  shared = false;
  sources = [ ./config.cc ];
}