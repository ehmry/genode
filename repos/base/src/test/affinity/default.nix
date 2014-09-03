/*
* \brief  Test to start threads on all available CPUs
* \author Emery Hemingway
* \date   2014-08-11
*/

{ build, base }:

build.test {
  name = "test-affinity";
  sources = [ ./main.cc ];
  libs = [ base.lib.base ];
  includeDirs = base.includeDirs;
}