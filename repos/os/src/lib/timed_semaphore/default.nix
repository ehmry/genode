/*
 * \author Emery Hemingway
 * \date   2014-09-22
 */

{ tool }: with tool;
{ alarm }:

buildLibrary {
  name = "timed_semaphore";
  libs = [ alarm ];
  sources = [ ./timed_semaphore.cc ];
}