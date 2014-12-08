/*
 * \brief  Entry expression for runs
 * \author Emery Hemingway
 * \date   2014-09-16
 */

{ system, tool, pkgs }:

let

  spec =
    if system == "i868-linux" then import specs/x86_32-linux.nix else
    if system == "x86_64-linux" then import specs/x86_64-linux.nix else
    if system == "x86_32-nova"  then import specs/x86_32-nova.nix  else
    if system == "x86_64-nova"  then import specs/x86_64-nova.nix  else
    abort "unknown system type ${system}";

  linuxRun = import ./repos/base-linux/tool/run { inherit tool pkgs; };
  novaRun  = import ./repos/base-nova/tool/run  { inherit tool pkgs; };

  run =
      if system == "i686-linux" then linuxRun else
      if system == "x86_64-linux" then linuxRun else
      if system == "x86_32-nova" then novaRun else
      if system == "x86_64-nova" then novaRun else
      abort "no run environment for ${system}";

  callRun = f:
    f (builtins.intersectAttrs
        (builtins.functionArgs f)
        { inherit spec run pkgs; }
      );

  importRun = p: callRun (import p);

  hasSuffixNix = tool.hasSuffix ".nix";
in
builtins.listToAttrs (builtins.concatLists (map
  (dir:
    let
      dir' = dir + "/run";
      set = builtins.readDir dir';
    in
    builtins.filter (x: x != null) (map
      (fn:
        if hasSuffixNix fn then
          { name = tool.replaceInString ".nix" "" fn;
            value = importRun (dir'+"/${fn}");
          }
        else null
      )
      (builtins.attrNames set)
    )
  )

  [ ./repos/base
    ./repos/base-linux
    ./repos/base-nova
    ./repos/os
    ./repos/libports
  ]
))
