/*
 * \brief  
 * \author Emery Hemingway
 * \date   2014-08-10
 */

{ tool }: with tool;

{ config }:

buildLibrary {
  name = "config_args";
  shared = false;
  sources = [ ./config_args.cc ];
  libs = [ config ];
}