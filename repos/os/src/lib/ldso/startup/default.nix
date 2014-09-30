/*
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ tool }: with tool;
{ }:

buildLibrary {
  name = "ldso-startup";
  sources = [ ./startup.cc ];
}