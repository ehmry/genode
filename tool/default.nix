/*
 * \brief  Tools and utility functions
 * \author Emery Hemingway
 * \date   2014-09-30
 */

{ system ? builtins.currentSystem
, nixpkgs ? import <nixpkgs> { }
}:

let tool = rec {
  inherit nixpkgs;

  replaceInString =
  a: b: s:
  let
    al = builtins.stringLength a;
    bl = builtins.stringLength b;
    sl = builtins.stringLength s;
  in
  if al == 0 then s else
  if sl == 0 then "" else
  if ((builtins.substring 0 al s) == a) then
    b+(replaceInString a b (builtins.substring al sl s))
  else
    (builtins.substring 0 1 s) + (replaceInString a b (builtins.substring 1 sl s));

  bootImage = import ./boot-image { inherit nixpkgs; };

  filterOut = filters: filteree:
    builtins.filter (e: !(builtins.elem (builtins.baseNameOf e) filters)) filteree;

  # if anti-quotation was used rather than concatenation,
  # the result would be a string containing a store path,
  # not the original absolute path
  fromDir =
  dir: names:
  assert builtins.typeOf dir == "path";
  map (name: [ (dir+"/${name}") name ]) names;

  fromPath = path: [ [ path (baseNameOf (toString path)) ] ];
  fromPaths = paths: map (p: [ p (baseNameOf (toString p)) ]) paths;

  newDir =
  name: contents:
  derivation {
    inherit name contents;
    system = builtins.currentSystem;
    builder = shell;
    PATH="${nixpkgs.coreutils}/bin";
    args = [ "-e" "-c" ''
      mkdir -p $out ; \
      for i in $contents; do cp -Hr $i $out; done
    '' ];
  };

  preparePort = import ./prepare-port { inherit nixpkgs; };

  shell = nixpkgs.bash + "/bin/sh";

  wildcard = 
  path: glob:
  let
    relativePaths = import (derivation {
      name = "files.nix";
      system = builtins.currentSystem;
      builder = shell;
      PATH="${nixpkgs.coreutils}/bin";
      inherit path glob;
      args = [ "-e" ./wildcard.sh ];
    });
  in
  map (rp: (path+"/${rp}")) relativePaths;

  # Utility functions for gathering sources.
  fromGlob =
  dir: glob:
  let
    dirName = dir.name or "files";
  in
  import (derivation {
    name = dirName+"-from-glob.nix";
    system = builtins.currentSystem;
    builder = shell;
    args = [ "-e" ./from-glob.sh ];
    #PATH="${nixpkgs.coreutils}/bin";
    inherit dir glob;
  });

  mergeAttr =
  name: s1: s2:
  let
    a1 = builtins.getAttr name s1;
    a2 = builtins.getAttr name s2;
    type = builtins.typeOf a1;
  in
  assert type == (builtins.typeOf a2);
  if type == "set" then mergeSet a1 a2 else
  if type == "list" then a1 ++ a2 else
  if type == "string" then "${a1} ${a2}" else
  #if type == "int" then builtins.add a1 a2 else
  abort "cannot merge ${type}s";

  mergeSet = s1: s2:
    s1 // s2 // (builtins.listToAttrs (map 
      (name: { inherit name; value = mergeAttr name s1 s2; })
      (builtins.attrNames (builtins.intersectAttrs s1 s2))
    ));

  mergeSets =
  sets:
  if sets == [] then {} else
    mergeSet (builtins.head sets) (mergeSets (builtins.tail sets));
  
  ####
  # from Eelco Dolstra's nix-make

  findFile = fn: searchPath:
    if searchPath == [] then []
    else
      let
        sp = builtins.head searchPath;
        fn' = sp + "/${fn}";
      in
      if builtins.pathExists fn' then
        [ { key = fn'; relative = fn; } ]
      else findFile fn (builtins.tail searchPath);

  includesOf = main: derivation {
    name =
      if builtins.typeOf main == "path"
      then "${baseNameOf (toString main)}-includes"
      else "includes";
    system = builtins.currentSystem;
    builder = "${nixpkgs.perl}/bin/perl";
    args = [ ./find-includes.pl ];
    inherit main;
  };

  localIncludesOf = main: derivation {
    name =
      if builtins.typeOf main == "path"
      then "${baseNameOf (toString main)}-local-includes"
      else "local-includes";
    system = builtins.currentSystem;
    builder = "${nixpkgs.perl}/bin/perl";
    args = [ ./find-local-includes.pl ];
    inherit main;
  };

  findIncludes = main: path:
    map (x: [ x.key x.relative ]) (builtins.genericClosure {
      startSet = [ { key = main; relative = baseNameOf (toString main); } ];
      operator =
        { key, ... }:
        let
          includes = import (includesOf key);
          includesFound =
            nixpkgs.lib.concatMap
              (fn: findFile fn ([ (builtins.dirOf main) ] ++ path))
              includes;
        in includesFound;
    });

  findLocalIncludes = main: path:
    let path' = [ (builtins.dirOf main) ] ++ path; in
    map (x: [ x.key x.relative ]) (builtins.genericClosure {
      startSet = [ { key = main; relative = baseNameOf (toString main); } ];
      operator =
        { key, ... }:
        let
          includes = import (localIncludesOf key);
          includesFound =
            nixpkgs.lib.concatMap
              (fn: findFile fn path')
              includes;
        in includesFound;
    });

    includesSet =
    sources: path:
    builtins.listToAttrs (
     map
      ( main:
        { name = "includes_" + (baseNameOf (toString main));
          value = builtins.tail (findLocalIncludes main path);
        }
      )
      sources
    );

    ####

}; in tool // import ./build { inherit system tool; }
