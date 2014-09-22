/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ build }:
{ base }:

build.component {
  name = "test-signal";
  libs = [ base ];
  sources = [ ./main.cc ];
}