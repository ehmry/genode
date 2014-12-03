/*
 * \brief  Function to create a system image directory
 * \author Emery Hemingway
 * \date   2014-08-13
 */

{ nixpkgs }:
{ name, contents }:

derivation {
  name = name+"-boot-image";
  system = builtins.currentSystem;
  builder = nixpkgs.bash+"/bin/sh";
  args = [ "-e" ./system-image.sh ];
  PATH = nixpkgs.coreutils+"/bin";

  sources = map (x: x.source) contents;
  targets = map (x: x.target) contents;
}
