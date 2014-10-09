/*
* \brief  Test to start threads on all available CPUs
* \author Emery Hemingway
* \date   2014-08-11
*/

{ builder, ... }:
{ base }:

builder {
  name = "test-affinity";
  src = [ ./main.cc ];
  libs = [ base ];
}