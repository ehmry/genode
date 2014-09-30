/*
 * \brief  Function to compile a source file to an object file
 * \author Emery Hemingway
 * \date   2014-08-6
 */

{ build, common, shell }:

with build;

{ source, includeDirs, ccFlags, cxxFlags, ... } @ args:
derivation ({
  name = "object";
  system = builtins.currentSystem;

  inherit (build) cc cxx;
  inherit common;

  builder = shell;
  args = [ "-e" ./compile-object.sh ];

  verbose = builtins.getEnv "VERBOSE";
} // args)
