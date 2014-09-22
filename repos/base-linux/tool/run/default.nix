/*
 * \brief  Function to execute a Linux run
 * \author Emery Hemingway
 * \date   2014-08-11
 */

{ nixpkgs, pkgs }:

let
  bootImage = import ../boot-image { inherit nixpkgs; };
in

{ name, contents, testScript }:

let
  contents' = contents ++ [
    { target = "/";
      source = pkgs.core;
    }
    { target = "/";
      source = pkgs.init;
    }
  ];
in
derivation {
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

  inherit testScript;

  image_dir = bootImage { inherit name; contents = contents'; };
}