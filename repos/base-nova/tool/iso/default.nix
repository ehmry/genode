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
  #PATH = 
  #  "${nixpkgs.coreutils}/bin/:" +
  #  "${nixpkgs.cdrkit}/bin:" +
  #  "${nixpkgs.syslinux}/bin";


  bender = ../../../../tool/boot/bender;

  # add the rest of the components in the script
  syslinuxCfg =
    ''
      SERIAL 0
      DEFAULT NOVA

      LABEL NOVA
        KERNEL mboot.c32
        APPEND /hypervisor iommu serial --- /genode/core'';

  inherit (nixpkgs) cdrkit syslinux;
  inherit (tool) genodeEnv;
  inherit (tool.genodeEnv) objcopy;

  image_dir = bootImage { inherit name contents; };
}
