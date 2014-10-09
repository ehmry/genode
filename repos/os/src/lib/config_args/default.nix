/*
 * \brief  
 * \author Emery Hemingway
 * \date   2014-08-10
 */

{ builder, ... }:
{ config }:

builder {
  name = "config_args";
  src = [ ./config_args.cc ];
  libs = [ config ];
}