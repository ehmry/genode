/*
 * \brief  Tools and utility functions
 * \author Emery Hemingway
 * \date   2014-09-30
 */

{ spec ? import ../spec { system = builtins.currentSystem; } }:

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
  hasDotHH = s: substring (sub (stringLength s) 3) 3 s == ".hh";
  hasDotHPP = s: substring (sub (stringLength s) 4) 4 s == ".hpp";

  ##
  # Filter out everything but *.h on a path.
  # Prevents files that exist alongside headers from changing header path hash.
  filterHeaders = dir: filterSource
    (path: type: hasDotH path || hasDotHH path || hasDotHPP path || type == "directory")
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

  ##
  # Create a bootable ISO.
  iso =
  { name, contents, kernel, kernelArgs }:
  shellDerivation {
    name = "${name}.iso";
    script = ./iso.sh;
    PATH="${nixpkgs.coreutils}/bin:${nixpkgs.cdrkit}/bin:${nixpkgs.binutils}/bin";
    inherit kernel kernelArgs;
    inherit (nixpkgs) syslinux cdrkit;
    sources = map (x: x.source) contents;
    targets = map (x: x.target) contents;
  };

  ##
  # Generate a contents list of runtime libraries for a package.
  # This will go away as tool.runtime matures.
  libContents = contents: builtins.concatLists (map (
    content:
      map (source: { target = "/"; inherit source; }) content.source.runtime.libs or []
  ) contents);

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


# START CRAZY TOWN

  # Appends string context from another string
  addContextFrom = a: b: substring 0 0 a + b;

  # Compares strings not requiring context equality
  # Obviously, a workaround but works on all Nix versions
  eqStrings = a: b: addContextFrom b a == addContextFrom a b;


  ##
  # Cut a string with a separator and produces a list of strings which were
  # separated by this separator. e.g.,
  # `splitString "." "foo.bar.baz"' returns ["foo" "bar" "baz"].
  # From nixpkgs.
  splitString = _sep: _s:
    let
      sep = addContextFrom _s _sep;
      s = addContextFrom _sep _s;
      sepLen = stringLength sep;
      sLen = stringLength s;
      lastSearch = sLen - sepLen;
      startWithSep = startAt:
        substring startAt sepLen s == sep;

      recurse = index: startAt:
        let cutUntil = i: [(substring startAt (i - startAt) s)]; in
        if index < lastSearch then
          if startWithSep index then
            let restartAt = index + sepLen; in
            cutUntil index ++ recurse restartAt restartAt
          else
            recurse (index + 1) startAt
        else
          cutUntil sLen;
    in
      recurse 0 0;
  ##
  # What I thought builtins.match would do.
  matchPattern = pat: str:
    concatLists (
      map
        ( l:
          let m = match pat l; in
          if m == null then [] else m
        )
        (splitString "\n" str)
    );

  ##
  # Generate a set of local ("") and system (<>)
  # preprocessor include directives from a file.
  relativeIncludes = file:
    let
      matches = pattern: lines:
        concatLists (filter (x: x != null) (map (match pattern) lines));
      lines = splitString "\n" (readFile file);
    in
    { local  = matches ''.*#include\s*"([^>]*)".*'' lines;
      system = matches ''.*#include\s*<([^>]*)>.*'' lines;
    };

  ##
  # Find a file in a set of directories.
  findFile' = key: dirs:
    if dirs == [] then null else
    let abs = (builtins.head dirs) +"/${key}"; in
    if builtins.pathExists abs then abs
    else findFile' key (builtins.tail dirs);

  ##
  # Generate a set of relative to absolute include mappings from a file.
  # This set includes a mapping from the orginal file basename to its
  # absolute path.
  #
  # The genericClosure primative applies an operation to a list of sets that
  # contain the attribute 'key'. This operation returns a similar list of sets,
  # and genericClosure appends elements of that list that contain a key
  # that does not already exist in the previous set. All sets returned by this
  # operation contain a function to resolve the relative path at 'key' into an
  # absolute one at 'abs', and a function to parse the absolute path at 'abs'
  # into a list of relative includes at 'inc'. GenericClosure discards any set
  # with a relative path at 'key' that it has already been seen, and thus due
  # to lazy evaulation, no relative path is resolved or parsed twice.
  #
  includesOfFile = file: searchPath:
    let
      concat = sets:
        if sets == [] then {} else
        let x = head sets; in
        (if x.abs == null then {} else { "${x.key}" = x.abs; }) // concat (tail sets);
    in
    concat (genericClosure {
      startSet = [ { key = baseNameOf (toString file); abs = file; inc = relativeIncludes file; } ];
      operator =
        { key, abs, inc }: if abs == null then [] else let abs' = abs; in
          (map 
            (key: rec { inherit key; abs = (findFile' key searchPath); inc = relativeIncludes abs; })
            inc.system)
          ++
          (map
            (key: rec { inherit key; abs = (findFile' key (searchPath++[ (dirOf abs') ])); inc = relativeIncludes abs; })
            inc.local);
    });

  includesDerivation = searchPath: file:
    let
      bn = baseNameOf (toString file);
      mappings = removeAttrs (includesOfFile file searchPath) [ bn ];
    in
    derivation {
      name = bn+"-includes";
      system = builtins.currentSystem;
      builder = shell;
      args = [ "-e" ./build/include.sh ];
      relative = builtins.attrNames  mappings;
      absolute = builtins.attrValues mappings;
    };

############################################################
# END CRAZY STUFF


}; in tool // import ./build { inherit spec tool; }
