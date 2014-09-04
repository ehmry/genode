{ build, common, compileObject, transformBinary, shell }:

let
  result = derivation {
    name = "build-component";
    system = builtins.currentSystem;

    inherit common;
    setup = ./setup.sh;
    builder = shell;
    args = [ "-e" ../common/sub-builder.sh ];
  };
in

{ name
, sources
, binaries ? []
, includeDirs
, libs
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

  cxxLinkOpt = 
    (args.cxxLinkOpt or build.cxxLinkOpt)
    ++
    (map (x: "-Wl,${x}") (args.ldOpt or build.ldOpt))
    ++
   [ "-nostdlib -Wl,-nostdlib" ];

in
derivation ( args // {
  inherit name;
  component = result;
  system = result.system;

  objects =
    map (source: compileObject { inherit source includeDirs ccFlags cxxFlags; }) sources
    ++
    map (binary: transformBinary { inherit binary; }) binaries;

  builder = shell;
  args = [ "-e" (args.builder or ./default-builder.sh) ];

  includes = map (d: "-I${d}") includeDirs;

  phases = ( args.phases or [ "linkPhase" ] );

  inherit (build)
    cc cxx 
    ldScriptDyn ldScriptSo;

  inherit (build.spec) ccMarch ldMarch;

  inherit cxxLinkOpt;

  ldTextAddr = args.ldTextAddr or build.spec.ldTextAddr or "";
  ldScriptStatic = args.ldScriptStatic or build.spec.ldScriptStatic or [ ];

  searchLibs = libs;

  verbose = builtins.getEnv "VERBOSE";
})