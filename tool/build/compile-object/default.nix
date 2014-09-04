/*
 * \brief  Function to compile a source file to an object file
 * \author Emery Hemingway
 * \date   2014-08-6
 */

{ build, common, shell }:

with build;

{ source, includeDirs, ccFlags, cxxFlags }:
derivation {
  name = "object";
  system = builtins.currentSystem;

  inherit (build) cc cxx;
  inherit common source ccFlags cxxFlags;
  includeDirs = includeDirs ++ [ "${toolchain}/lib/gcc/${spec.target}/${toolchain.version}/include" ];

  builder = shell;
  args = [ "-e" ./compile-object.sh ];

  verbose = builtins.getEnv "VERBOSE";

}
