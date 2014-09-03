{ spec, common, compiler, compile, transformBinary, shell }:

let
  result = derivation
    { name = "genode-build-program";
      system = builtins.currentSystem;

      inherit common;

      setup = ./setup.sh;

      builder = shell;
      args = [ "-e" ../common/sub-builder.sh ];
    };

in
{ sources
, binaries ? []
, includeDirs
, flags ? []
, libs
, ... } @ args:
derivation ( args // {
  program = result;
  system = result.system;

  objects =
    map (source: compile { inherit source includeDirs flags; }) sources
    ++
    map (binary: transformBinary { inherit binary; }) binaries;

  builder = shell;
  args = [ "-e" (args.builder or ./default-builder.sh) ];

  includes = map (d: "-I${d}") includeDirs;

  phases = ( args.phases or [ "linkPhase" ] );

  inherit (compiler)
    cc cxx cxxLinkOpt
    ldScriptDyn ldScriptSo;

  inherit (spec) ccMarch;

  ldTextAddr = args.ldTextAddr or spec.ldTextAddr or "";
  ldScriptStatic = args.ldScriptStatic or spec.ldScriptStatic or [ ];

  searchLibs = libs;

  verbose = builtins.getEnv "VERBOSE";
})