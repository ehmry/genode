/*
 * \brief  Function to execute a NOVA run
 * \author Emery Hemingway
 * \date   2014-08-16
 */

 { tool, pkgs }:

with tool;

{ name, contents
, automatic ? true
, testScript }:

let
  contents' =
    ( map ({ target, source }:
        { target = "/${target}"; inherit source; })
        contents
    ) ++
    [
      { target = "/";
        source = pkgs.core;
      }
      { target = "/";
        source = pkgs.init;
      }
      { target = "/";
        source = pkgs.libs.ld;
      }
    ];

  diskImage = tool.iso {
    inherit name; contents = contents';
    kernel = "${pkgs.hypervisor}/hypervisor";
    kernelArgs = "iommu serial";
  };

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
      ./nova-auto.exp
      ./nova.exp
    ];

  inherit automatic diskImage testScript;

  userScript =
    ''
      #!${nixpkgs.expect}/bin/expect
      set disk_image ${diskImage}
      source ${ ../../../../tool/run}
      source ${./nova.exp}

      ${testScript}
    '';
}
