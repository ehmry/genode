/*
 * \brief  Function to execute a Linux run
 * \author Emery Hemingway
 * \date   2014-08-11
 */

{ tool, pkgs }:

with tool;

{ name, contents
, automatic ? true
, testScript }:

let
  contents' =
    contents ++
    (tool.libContents contents) ++
    [ { target = "/";
        source = pkgs.core;
      }
      { target = "/";
        source = pkgs.init;
      }
      { target = "/";
        source = pkgs.libs.ld;
      }
    ];

  imageDir = bootImage { inherit name; contents = contents'; };
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
      ./linux-auto.exp
      ./linux.exp
    ];

  inherit automatic imageDir testScript;

  userScript =
    ''
      #!${nixpkgs.expect}/bin/expect
      set image_dir ${imageDir}
      source ${ ../../../../tool/run}
      source ${./linux.exp}

      ${testScript}
    '';
}
