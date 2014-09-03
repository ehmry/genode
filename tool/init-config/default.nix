/*
 * \brief  Function to generate an init config
 * \author Emery Hemingway
 * \date   2014-08-13
 *
 * Really I could just spit out a config with some
 * template and antiquotations.
 *
 */

{ nixpkgs }:
{ parentProvides, children, ... } @ args:

let
  raw = builtins.toFile "raw-config" (builtins.toXML args);
  stylesheet = ./stylesheet.xsl;
in
derivation {
  name = "config";
  system = builtins.currentSystem;
  builder = nixpkgs.bash+"/bin/sh";
  args = [ "-c" "${nixpkgs.libxslt}/bin/xsltproc -o $out ${stylesheet} ${raw}" ];
}