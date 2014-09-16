/*
 * \brief  Expression for the Linux implementation of base packages
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ build, base }:

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

  core = import ./src/core { inherit build base repo; };

}