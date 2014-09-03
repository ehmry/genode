/*
 * \brief  Function to execute a Linux run
 * \author Emery Hemingway
 * \date   2014-08-11
 */

{ spec, nixpkgs, tool }:

{ name, bootInputs
, waitRegex ? "", waitTimeout ? 0
, ... } @ args:

derivation (args // {
  name = name+"-run";
  system = builtins.currentSystem;
  preferLocalBuild = true;

  PATH = 
    "${nixpkgs.coreutils}/bin:" +
    "${nixpkgs.which}/bin:" +
    "${nixpkgs.findutils}/bin:" +
    "${nixpkgs.libxml2}/bin:";

  builder = nixpkgs.expect + "/bin/expect";
  args =
   [ "-nN"
     ../../../../tool/run-nix-setup.exp
     # setup.exp will source the files that follow
     ../../../../tool/run
     ./linux.exp
   ];

  inherit waitRegex waitTimeout;

  image_dir = tool.bootImage {
    inherit name;
    inputs = bootInputs;
  };

})