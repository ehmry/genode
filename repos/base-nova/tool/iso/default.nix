/*
 * \brief  Function to build an ISO image
 * \author Emery Hemingway
 * \date   2014-08-17
 *
 * This will move up into genode/tool if it becomes 
 * more general, otherwise it lives here.
 *
 * I consider this poorly implemented for now,
 * I would like better boot config generation than 
 * 'echo foo >> menu.lst'
 */

{ nixpkgs, tool }:

let
  shell = nixpkgs.bash + "/bin/sh";

  inputGraft =
    name: files:
    map (file: "${name}=${file}") files;
in

{ name, hypervisor, genodeImage}:

derivation {
  name = "${name}.iso";
  system = builtins.currentSystem;

  builder = shell;
  args = [ "-e" ./iso.sh ];
  inherit hypervisor genodeImage;
  genisoimage = nixpkgs.cdrkit + "/bin/genisoimage";
  PATH = "${nixpkgs.coreutils}/bin/";
  boot = ../../../../tool/boot;
}