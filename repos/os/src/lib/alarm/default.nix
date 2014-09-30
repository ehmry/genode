/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ tool }: with tool;

# no library dependencies
{ }:

buildLibrary {
  name = "alarm";
  shared = false;
  sources = [ ./alarm.cc ];
}