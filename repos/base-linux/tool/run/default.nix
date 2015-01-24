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

  dynamicLinking = tool.anyShared (map (x: x.source) contents');

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

  inherit imageDir testScript;

  automatic =
    if dynamicLinking
    then builtins.trace "${name}-run contains shared libraries, not executing." false
    else automatic;

  userScript =
    ''
      #!${nixpkgs.expect}/bin/expect
      set image_dir ${imageDir}
      set name ${name}
      source ${ ../../../../tool/run}
      source ${./linux.exp}

      set temp_dir /tmp/$name-run-[pid]
      file mkdir $temp_dir
      foreach f [glob $image_dir/*] {
      	file link $temp_dir/[file tail $f] $f
      }

      cd $temp_dir
      ${testScript}
      file delete $temp_dir
    '';
}
