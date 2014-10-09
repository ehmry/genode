/*
 * \brief  Function to build an ISO image
 * \author Emery Hemingway
 * \date   2014-08-17
 *
 * This will move up into genode/tool if it becomes 
 * more general, otherwise it lives here.
 */

{ tool }:
{ name, contents }:

with tool;

derivation {
  name = "${name}.iso";
  system = builtins.currentSystem;

  builder = shell;
  args = [ "-e" ./iso.sh ];
  PATH = 
    "${nixpkgs.coreutils}/bin/:" +
    "${nixpkgs.gnused}/bin:" +
    "${nixpkgs.xorriso}/bin:" +
    "${nixpkgs.grub2}/bin";

  bender = ../../../../tool/boot/bender;

  # add the rest of the components in the script
  grubCfg = ''
    serial
    terminal_output serial
    set timeout=0

    menuentry "Genode on NOVA - ${name}" {
      multiboot /boot/bender
      module /boot/hypervisor iommu serial
      module /genode/core
      module /genode/config
      module /genode/init
  '';

  image_dir = bootImage { inherit name contents; };
}