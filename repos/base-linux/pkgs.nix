/*
 * \brief  Expression for the Linux implementation of base packages
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ tool, base }:

let
  repo = rec
    { sourceDir = ./src; includeDir = ./include;
      includeDirs =
        [ includeDir
          ./src/platform
        ];
    };

in
repo // {

  core = import ./src/core {
    inherit (tool) build;
    inherit base repo;
  };

}