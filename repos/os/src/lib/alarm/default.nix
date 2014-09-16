/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build }:

# no library dependencies
{ }:

build.library {
  name = "alarm";
  shared = false;
  sources = [ ./alarm.cc ];
}