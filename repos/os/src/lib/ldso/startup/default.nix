/*
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ build }:
{ }:

build.library {
  name = "ldso-startup";
  sources = [ ./startup.cc ];
}