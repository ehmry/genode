/*
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ builder, ... }:
{ }: # no library dependencies

builder { name = "server"; sources = [ ./server.cc ]; }