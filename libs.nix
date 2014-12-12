/*
 * \brief  Entry expression into libraries
 * \author Emery Hemingway
 * \date   2014-09-11
 */

{ system ? builtins.currentSystem
, tool ? import ./tool { inherit system; }
}:

let
  callLibrary = extraAttrs: f:
    f (
      builtins.intersectAttrs
        (builtins.functionArgs f)
        ( tool // libs //
          { inherit (tool) linkStaticLibrary;
            linkSharedLibrary = tool.linkSharedLibrary {
              inherit (libs') ldso-startup;
            };
          } //
          extraAttrs
        )
    );

  libs' = tool.mergeSets (
    map
      (repo: import (./repos + "/${repo}/libs.nix") {
        inherit tool callLibrary;
      })
      [ "base" "os" "demo" "libports" ]
  );

  libs = libs' // {
    baseLibs = with libs';
      [ base-common base startup cxx
        timed_semaphore alarm config syscall
      ];
  };

in libs
