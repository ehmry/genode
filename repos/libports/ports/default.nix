{ tool ? import ../../../tool { } }:

let
  importPort = path:
    let f = (import path);
    in f (builtins.intersectAttrs (builtins.functionArgs f) (tool.nixpkgs // tool) );

  hasSuffixNix = tool.hasSuffix ".nix";
in
builtins.listToAttrs (
  builtins.filter
    (x: x != null)
    (map
      (fn:
         if hasSuffixNix fn && fn != "default.nix" then
           { name = tool.replaceInString ".nix" "" fn;
             value = importPort (../ports + "/${fn}");
           }
         else null
      )
      (builtins.attrNames (builtins.readDir ../ports))
    )
)
