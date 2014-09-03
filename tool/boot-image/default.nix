/*
 * \brief  Function to create a boot image directory
 * \author Emery Hemingway
 * \date   2014-08-13
 */

{ nixpkgs }:
{ name, inputs }:

derivation {
  name = name+"-boot-image";
  system = builtins.currentSystem;
  builder = nixpkgs.bash+"/bin/sh";
  args = [ "-e" ./boot-image.sh ];
  inherit inputs;
  PATH = nixpkgs.coreutils+"/bin";
}