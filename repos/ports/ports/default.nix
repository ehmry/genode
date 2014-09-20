/*
 * \brief  External source code for ports
 * \author Emery Hemingway
 * \date   2014-09-19
 */

{ tool ? import ../../../tool { } }:

let
  callPort = path:
    let f = (import path);
    in f (builtins.intersectAttrs (builtins.functionArgs f) (tool.nixpkgs // tool) );
in
{
  dosbox = callPort ./dosbox.nix;
}