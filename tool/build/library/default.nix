{ spec, common, compiler, compile, transformBinary, shell }:

let
  result = derivation
    { name = "genode-build-library";
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
, flags ? []
, libs ? []
, entryPoint ? "0x0"
, ... } @ args:
derivation ( args // {
  library = result;
  system = result.system;

  objects = 
    map ( source: compile { inherit source includeDirs flags; }) sources
    ++
    map (binary: transformBinary { inherit binary; }) binaries;

  inherit libs entryPoint;

  inherit (compiler) cc cxx ar ld ldOpt objcopy;
  inherit (spec) ccMarch ldMarch;

  builder = shell;
  args = [ "-e" (args.builder or ./default-builder.sh)];

  phases = args.phases or (if shared then [ "mergeSharedPhase" ] else [ "mergeStaticPhase" ]) ++ [ "fixupPhase" ];

  verbose = builtins.getEnv "VERBOSE";
})