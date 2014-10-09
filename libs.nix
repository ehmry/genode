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
        (libs // extraAttrs )
    );

  importLibs = p: import p { inherit tool callLibrary; };

  base     = importLibs ./repos/base/libs.nix;
  os       = importLibs ./repos/os/libs.nix;
  libports = importLibs ./repos/libports/libs.nix;

  #demo = import ./repos/demo/libs.nix {
  #  inherit tool callLibrary;
  #  includes = baseIncludes ++ osIncludes;
  #};

  #
  #  inherit tool callLibrary;
  #  includes = baseIncludes ++ osIncludes ++ libportsIncludes;
  #};

  libs' = tool.mergeSets [ base os libports ]; #demo
  libs  = libs' // {
    baseLibs = with libs;
      [ base-common base.base startup cxx
        timed_semaphore alarm config syscall
      ];
  };

in libs
