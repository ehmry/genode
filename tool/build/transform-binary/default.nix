/*
 * \brief  Function to transform binary data into an object file
 * \author Emery Hemingway
 * \date   2014-08-14
 */

{ spec, compiler, common, shell }:

{ binary }:
derivation {
  name = "${spec.system}-binary";
  system = builtins.currentSystem;
  inherit common binary;
  inherit (compiler) as asOpt;
  builder = shell;
  args = [ "-e" ./transform-binary.sh ];

  verbose = builtins.getEnv "VERBOSE";
}