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
, ... } @ args:

let
  ccFlags = [
    (args.ccOpt or build.ccOpt)
    (args.ccOLevel or build.ccOLevel)
    (args.ccOptDep or build.ccOptDep)
    (args.ccWarn or build.ccWarn)
  ];
  cxxFlags = [
    (args.ccCxxOpt or build.ccCxxOpt)
  ];
in
derivation ( args // {
  library = result;
  system = result.system;

  objects = 
    map (source: compileObject { inherit source includeDirs ccFlags cxxFlags; }) sources
    ++
    map (binary: transformBinary { inherit binary; }) binaries;

  inherit libs entryPoint;

  inherit cc cxx ar ld ldOpt objcopy;
  inherit (spec) ccMarch ldMarch;

  builder = shell;
  args = [ "-e" (args.builder or ./default-builder.sh)];

  phases = args.phases or (if shared then [ "mergeSharedPhase" ] else [ "mergeStaticPhase" ]) ++ [ "fixupPhase" ];

  verbose = builtins.getEnv "VERBOSE";
})