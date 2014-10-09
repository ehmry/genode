/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ genodeEnv }:

genodeEnv.mkLibrary {
  name = "config"; sources = genodeEnv.fromPath ./config.cc;
}
