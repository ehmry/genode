/*
 * \brief  Entry expression for runs
 * \author Emery Hemingway
 * \date   2014-09-16
 */

{ system ? builtins.currentSystem
, spec ? import ./spec { inherit system; }
, tool ? import ./tool  { inherit spec; }
, pkgs ? import ./pkgs.nix { inherit system spec tool; }
}:

with builtins;

let
  run =
      if system == "i686-linux" then linuxRun else
      if system == "x86_64-linux" then linuxRun else
      if system == "i686-nova" then novaRun else
      if system == "x86_64-nova" then novaRun else
      abort "no run environment for ${system}";

  # The new rutime tools.
  runtime = import ./tool/runtime { inherit spec tool pkgs; };

  callRun = f:
    f (builtins.intersectAttrs
        (builtins.functionArgs f)
        { inherit spec tool run runtime pkgs; }
      );

  hasSuffixNix = tool.hasSuffix ".nix";
  dropNix = tool.dropSuffix ".nix";

  repos = readDir ./repos;
  import' = p: callRun (import p);

in
listToAttrs (filter (x: x != null) (concatLists (map
  (repo:
    let
      runDirName = ./repos + "/${repo}/run";
      runDir =
        if pathExists runDirName
        then readDir runDirName
        else {};
    in
    map
      (fn:
        let type = getAttr fn runDir; in
        if type == "regular" && hasSuffixNix fn
        then { name = dropNix fn; value = import' (runDirName + "/${fn}"); }
        else null
      )
      (attrNames runDir)
  )
  (attrNames repos)
)))

