{ spec }:

with builtins;

let
  repos = readDir ./repos;
  import' = p: (import p) { inherit spec; };
in
listToAttrs (filter (x: x != null) (map
  (name':
    let
      name = name'+"Includes";
      include  = ./repos + "/${name'}/include";
      exprFile = include + "/default.nix";
    in
    if pathExists exprFile
    then { inherit name; value = import' exprFile; } else
    if pathExists include
    then { inherit name; value = [ include ]; } else
    null
  )
  (attrNames repos)
))