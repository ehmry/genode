/*
 * \brief  
 * \author Emery Hemingway
 * \date   2014-08-10
 */

{ build }:

{ config }:

build.library {
  name = "config_args";
  shared = false;
  sources = [ ./config_args.cc ];
  libs = [ config ];
}