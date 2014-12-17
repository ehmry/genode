/*
 * \brief  Function to execute a NOVA run
 * \author Emery Hemingway
 * \date   2014-08-16
 */

 { tool, pkgs }:

with tool;

let iso = import ../iso { inherit tool; }; in

{ name, contents, graphical ? false, testScript }:

let
  contents' =
    ( map ({ target, source }:
        { target = "/genode/${target}"; inherit source; })
        contents
    ) ++
    [
      { target = "/genode";
        source = pkgs.core;
      }
      { target = "/genode";
        source = pkgs.init;
      }
      { target = "/genode";
        source = pkgs.libs.ld;
      }
      { target = "/";
        source = pkgs.hypervisor;
      }
    ];

in
derivation {
  name = name+"-run";
  system = builtins.currentSystem;
  preferLocalBuild = true;

  PATH =
    "${nixpkgs.coreutils}/bin:" +
    "${nixpkgs.findutils}/bin:" +
    "${nixpkgs.qemu}/bin";

  builder = nixpkgs.expect + "/bin/expect";
  args =
    [ "-nN"
      ../../../../tool/run-nix-setup.exp
      # setup.exp will source the files that follow
      ../../../../tool/run
      ./nova.exp
    ];

  inherit graphical testScript;

  diskImage = iso { inherit name; contents = contents'; };
}
