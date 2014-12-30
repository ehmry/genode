/*
 * \brief  Tools and utility functions
 * \author Emery Hemingway
 * \date   2014-09-30
 */

{ spec ? import ../specs { system = builtins.currentSystem; } }:

with builtins;

let tool = rec {

  nixpkgs =  import <nixpkgs> { };

  nixpkgsCross = import <nixpkgs> {
    crossSystem =
      { config = spec.target;
        libc = "libc_noux";
      };
  };

  ##
  # Determine if any of the following libs are shared.
  anyShared = libs:
    let h = head libs; in
    if libs == [] then false else
    if h.shared or false then true else anyShared (tail libs);

  ##
  # Drop a suffix from the end of a string.
  dropSuffix = suf: str:
    let
      strL = stringLength str;
      sufL = stringLength suf;
    in
    if lessThan strL sufL || substring (sub strL sufL) strL str != suf
    then abort "${str} does not have suffix ${suf}"
    else substring 0 (sub strL sufL) str;

  ##
  # Generate a list of file paths from a directory and
  # filenames.
  fromDir =
  dir: names:
  #assert typeOf dir == "path";
  map (name: dir+"/"+name) names;

  ##
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

  fromPath = path: [ [ path (baseNameOf (toString path)) ] ];
  fromPaths = paths: map (p: [ p (baseNameOf (toString p)) ]) paths;

  ##
  # Filter out libs that are not derivations
  filterFakeLibs = libs: filter (lib: hasAttr "shared" lib) libs;

  ##
  # Test if a string ends in '.h'.
  hasDotH = s: substring (sub (stringLength s) 2) 2 s == ".h";

  ##
  # Filter out everything but *.h on a path.
  # Prevents files that exist alongside headers from changing header path hash.
  filterHeaders = dir: filterSource
    (path: type: hasDotH path || type == "directory")
    dir;

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

  ##
  # Recursively find libraries.
  findLibraries = libs:
    let
      list = map (lib: { key = lib.name; inherit lib; });
    in
    map (x: x.lib) (genericClosure {
      startSet = list libs;
      operator = { key, lib }: list lib.libs or [];
    });

  ##
  # Recursively find libraries to link.
  findLinkLibraries = libs:
    let
      list = libs: map
        (lib: { key = lib.name; inherit lib; })
        (filter (lib: hasAttr "drvPath" lib) libs);
    in
    map (x: x.lib) (genericClosure {
      startSet = list libs;
      operator =
        { key, lib }:
        if lib.shared then []
        else list lib.libs or [];
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

  ##
  # Recursively find shared libraries.
  findRuntimeLibraries = libs:
    let
      filter = libs: builtins.filter (lib: lib.shared) libs;
      list = libs:
        map (lib: { key = lib.name; inherit lib; }) libs;
    in
  filter (map (x: x.lib) (genericClosure {
    startSet = list libs;
    operator =
      { key, lib }:
      list ([lib ] ++ lib.libs);
  }));

  ##
  # Determine if a string has the given suffix.
  hasSuffix = suf: str:
    let
      strL = stringLength str;
      sufL = stringLength suf;
    in
    if lessThan strL sufL then false else
      substring (sub strL sufL) strL str == suf;

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

  ##
  # Merge an attr between two sets.
  mergeAttr =
  name: s1: s2:
  let
    a1 = getAttr name s1;
    a2 = getAttr name s2;
    type1 = typeOf a1;
    type2 = typeOf a2;
  in
  if type1 == "null" then a2 else if type2 == "null" then a1 else
  if type1 != type2 then abort "cannot merge ${name}s of type ${type1} and ${type2}" else
  if type1 == "set" then mergeSet a1 a2 else
  if type1 == "list" then a1 ++ a2 else
  if type1 == "string" then "${a1} ${a2}" else
  #if type1 == "int" then add a1 a2 else
  abort "cannot merge ${type1} ${name} ${toString a1} ${toString a2}";

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

  ##
  # Generate a list of paths from a path and a shell glob.
  pathsFromGlob = dir: glob:
    let path = toString dir; in
    trace "FIXME: pathsFromGlob causes excessive hashing"
    import (derivation {
      name = "${baseNameOf path}-glob.nix";
      args = [ "-e" "-O" "nullglob" ./path-from-glob.sh ];
      inherit dir glob path;
    });

  preparePort = import ./prepare-port { inherit nixpkgs; };

  # Concatenate the named attr found in pkgs.
  propagate = attrName: pkgs:
    let
      pkg = head pkgs;
    in
    if pkgs == [] then [] else
    ( if hasAttr attrName pkg
      then getAttr attrName pkg
      else []
    ) ++
    (propagate attrName (tail pkgs));


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

  shell = nixpkgs.bash + "/bin/sh";

  # Save some typing when creating derivation that use our shell.
  shellDerivation = { script, ... } @ args:
    derivation ( (removeAttrs args [ "script" ]) //
      { system = builtins.currentSystem;
        builder = shell;
        args = [ "-e" script ];
      }
    );

  singleton = x: [x];

  # Bootability is not assured, so its really system image.
  systemImage = import ./system-image { inherit nixpkgs; };
  bootImage = systemImage; # get rid of this


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


}; in tool // import ./build { inherit spec tool; }
