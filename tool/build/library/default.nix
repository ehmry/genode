{ build, common, compileObject, transformBinary, shell }:

with build;

let
  result = derivation
    { name = "build-library";
      system = builtins.currentSystem;

      inherit common;
      setup = ./setup.sh;
      builder = shell;
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
, propagatedIncludes ? [ ]
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

  includeDirs = args.includeDirs ++
    (builtins.concatLists
      (map (lib: lib.propagatedIncludes ) libs));
in
derivation (args // {
  inherit shared;
  library = result;
  system = result.system;

  objects = 
    map (source: compileObject ({
      inherit source includeDirs ccFlags cxxFlags;
    } // compileArgs)) sources
    ++
    map (binary: transformBinary { inherit binary; }) binaries;

  inherit libs entryPoint;

  inherit cc cxx ar ld ldOpt objcopy ldFlags;
  inherit (spec) ccMarch;

  ldScriptSo = args.ldScriptSo or ../../../repos/os/src/platform/genode_rel.ld;

  builder = shell;
  args = [ "-e" (args.builder or ./default-builder.sh)];

  phases = args.phases or (if shared then [ "mergeSharedPhase" ] else [ "mergeStaticPhase" ]) ++ [ "fixupPhase" ];

  compileArgs = null; # only link in this environment
  verbose = builtins.getEnv "VERBOSE";

  # not used by the builder but by further dependencies
  # so actually this should be in a set that wraps this
  # derivation
  inherit propagatedIncludes;
})