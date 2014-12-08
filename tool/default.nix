/*
 * \brief  Tools and utility functions
 * \author Emery Hemingway
 * \date   2014-09-30
 */

{ system ? builtins.currentSystem
, nixpkgs ? import <nixpkgs> { }
}:

with builtins;

let tool = rec {
  inherit nixpkgs;

  shell = nixpkgs.bash + "/bin/sh";

  # Save some typing when creating derivation that use our shell.
  shellDerivation = { script, ... } @ args:
    derivation ( (removeAttrs args [ "script" ]) //
      { system = builtins.currentSystem;
        builder = shell;
        args = [ "-e" script ];
      }
    );

  ##
  # Determine if any of the following libs are shared.
  anyShared = libs:
    let h = head libs; in
    if libs == [] then false else
    if h.shared or false then true else anyShared (tail libs);

  dropSuffix = suf: str:
    let
      strL = stringLength str;
      sufL = stringLength suf;
    in
    if lessThan strL sufL || substring (sub strL sufL) strL str != suf
    then abort "${str} does not have suffix ${suf}"
    else substring 0 (sub strL sufL) str;

  ##
  # Determine if a string has the given suffix.
  hasSuffix = suf: str:
    let
      strL = stringLength str;
      sufL = stringLength suf;
    in
    if lessThan strL sufL then false else
      substring (sub strL sufL) strL str == suf;

  ##
  # Replace substring a with substring b in string s.
  replaceInString =
  a: b: s:
  let
    al = stringLength a;
    bl = stringLength b;
    sl = stringLength s;
  in
  if al == 0 then s else
  if sl == 0 then "" else
  if ((substring 0 al s) == a) then
    b+(replaceInString a b (substring al sl s))
  else
    (substring 0 1 s) + (replaceInString a b (substring 1 sl s));

  # Bootability is not assured, so its really system image.
  systemImage = import ./system-image { inherit nixpkgs; };
  bootImage = systemImage; # get rid of this

  filterOut = filters: filteree:
    filter (e: !(elem (baseNameOf e) filters)) filteree;

  # if anti-quotation was used rather than concatenation,
  # the result would be a string containing a store path,
  # not the original absolute path
  fromDir =
  dir: names:
  #assert typeOf dir == "path";
  map (name: dir+"/"+name) names;

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

  # Concatenate the propagatedIncludes found in pkgs.
  propagateIncludes = pkgs:
    let pkg = head pkgs; in
    if pkgs == [] then [] else
    (pkg.propagatedIncludes or []) ++ (propagateIncludes (tail pkgs));

  wildcard =
  path: glob:
  let
    relativePaths = import (shellDerivation {
      name = "files.nix";
      PATH="${nixpkgs.coreutils}/bin";
      script = ./wildcard.sh;
      inherit path glob;

    });
  in
  map (rp: (path+"/${rp}")) relativePaths;

  # Utility functions for gathering sources.
  fromGlob =
  dir: glob:
  let
    dirName = dir.name or baseNameOf (toString dir);
  in
  import (shellDerivation {
    name = "${dirName}-glob.nix";
    script = ./from-glob.sh;
    #PATH="${nixpkgs.coreutils}/bin";
    inherit dir glob;
  });

  # Generate a list of paths from a path and a shell glob.
  pathsFromGlob = dir: glob:
    let path = toString dir; in
    trace "FIXME: pathsFromGlob causes excessive hashing"
    import (derivation {
      name = "${baseNameOf path}-glob.nix";
      args = [ "-e" "-O" "nullglob" ./path-from-glob.sh ];
      inherit dir glob path;
    });

  ##
  # Merge an attr between two sets.
  mergeAttr =
  name: s1: s2:
  let
    a1 = getAttr name s1;
    a2 = getAttr name s2;
    type = typeOf a1;
  in
  assert type == (typeOf a2);
  if type == "set" then mergeSet a1 a2 else
  if type == "list" then a1 ++ a2 else
  if type == "string" then "${a1} ${a2}" else
  #if type == "int" then add a1 a2 else
  abort "cannot merge ${type} ${name} ${toString a1} ${toString a2}";

  ##
  # Merge two sets together.
  mergeSet = s1: s2:
    s1 // s2 // (listToAttrs (map
      (name: { inherit name; value = mergeAttr name s1 s2; })
      (attrNames (intersectAttrs s1 s2))
    ));

  ##
  # Merge a list of sets.
  mergeSets =
  sets:
  if sets == [] then {} else
    mergeSet (head sets) (mergeSets (tail sets));

  ####
  # from Eelco Dolstra's nix-make

  ##
  # Find a filename in a search path.
  findFile = fn: searchPath:
    if searchPath == [] then []
    else
      let
        sp = head searchPath;
        fn' = sp + "/${fn}";
      in
      if builtins.typeOf fn' != "path" then [] else
      if pathExists fn' then
        [ { key = fn'; relative = fn; } ]
      else findFile fn (tail searchPath);

  includesOf = main: derivation {
    name =
      if typeOf main == "path"
      then "${baseNameOf (toString main)}-includes"
      else "includes";
    system = currentSystem;
    builder = "${nixpkgs.perl}/bin/perl";
    args = [ ./find-includes.pl ];
    inherit main;
  };

  localIncludesOf = main: derivation {
    name =
      if typeOf main == "path"
      then "${baseNameOf (toString main)}-local-includes"
      else "local-includes";
    system = currentSystem;
    builder = "${nixpkgs.perl}/bin/perl";
    args = [ ./find-local-includes.pl ];
    inherit main;
  };

  findIncludes = main: path:
    map (x: [ x.key x.relative ]) (genericClosure {
      startSet = [ { key = main; relative = baseNameOf (toString main); } ];
      operator =
        { key, ... }:
        let
          includes = import (includesOf key);
          includesFound =
            nixpkgs.lib.concatMap
              (fn: findFile fn ([ (dirOf main) ] ++ path))
              includes;
        in includesFound;
    });

  findLocalIncludes = main: path:
    let path' = [ (dirOf main) ] ++ path; in
    map (x: [ x.key x.relative ]) (genericClosure {
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
    listToAttrs (
     map
      ( main:
        { name = "includes_" + (baseNameOf (toString main));
          value = tail (findLocalIncludes main path);
        }
      )
      sources
    );

    ####

}; in tool // import ./build { inherit system tool; }
