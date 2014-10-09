#{ tool, ... } @ arg:
#{
#  buildScripts = [
#    ( if (args.shared or false) then ../link-shared-library.sh
#      else ../link-static-build.sh
#    )
#  ];
#}

{ build }:

let
  result = derivation
    { name = "build-library";
      system = builtins.currentSystem;

      inherit (build) common;
      setup = ./setup.sh;
      builder = build.shell;
      args = [ "-e" ../common/sub-builder.sh ];
    };
in

{ name
, shared ? false
, sources
, binaries ? []
, includeDirs
, libs ? []
, entryPoint ? "0x0"
, compileArgs ? { }
#, propagatedIncludes ? [ ]
, ... } @ args:

let
  ccFlags = [
    (args.ccDef or null)
    (args.ccOpt or build.ccOpt)
    (args.ccOLevel or build.ccOLevel)
    (args.ccOptDep or build.ccOptDep)
    (args.ccWarn or build.ccWarn)
  ];
  cxxFlags = [
    (args.ccCxxOpt or build.ccCxxOpt)
  ];
  ldFlags = args.ldOpt or build.ldOpt;

  #includeDirs = args.includeDirs ++
  #  (builtins.concatLists
  #    (map (lib: lib.propagatedIncludes ) libs));

  objects =
    map (main: build.compileObject ({
      inherit main ccFlags cxxFlags;
      localIncludePath = includeDirs;
    } // compileArgs)) sources
    ++
    map (binary: build.transformBinary { inherit binary; }) binaries;
in
abort "build.library is deprecated"
derivation (args // {
  inherit shared;
  library = result;
  system = result.system;
  outputs = [ "out" "src" ];

  inherit objects;
  objectSources = map (o: o.src) objects;

  inherit libs entryPoint;

  inherit (build) cc cxx ar ld ldOpt objcopy verbose;
  inherit (build.spec) ccMarch;
  inherit ldFlags;

  ldScriptSo = args.ldScriptSo or ../../../repos/os/src/platform/genode_rel.ld;

  builder = build.shell;
  args = [ "-e" (args.builder or ./default-builder.sh)];

  phases = args.phases or (if shared then [ "mergeSharedPhase" ] else [ "mergeStaticPhase" ]) ++ [ "fixupPhase" ];

  compileArgs = null; # only link in this environment

  # not used by the builder but by further dependencies
  # so actually this should be in a set that wraps this
  # derivation
  #inherit propagatedIncludes;
})