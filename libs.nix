/*
 * \brief  Entry expression into libraries
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ system ? builtins.currentSystem
, spec ? import ./spec { inherit system; }
, tool ? import ./tool  { inherit spec; }
}:

let
  includes = import ./include.nix { inherit spec; };

  callLibrary = extraAttrs: f:
    f (
      builtins.intersectAttrs
        (builtins.functionArgs f)
        ( tool // libs //
          { inherit (tool) linkStaticLibrary;
            inherit spec;
            linkSharedLibrary = tool.linkSharedLibrary {
              inherit (libs') ldso-startup;
            };
          } //
          extraAttrs
        )
    );

  libs' = tool.mergeSets (
    map
      (repo: import (./repos + "/${repo}/libs.nix")
        ({ inherit spec tool callLibrary; } // includes)
      )
      [ "base" "os" "demo" "libports" "ports" "dde_ipxe" "gems" ]
  );

  libs = libs' // {
    baseLibs = with libs';
      [ base-common base startup cxx
        timed_semaphore alarm config syscall
      ];
  };

in libs
