{ build }:

let
  result = derivation {
    name = "build-component";
    system = builtins.currentSystem;

    inherit (build) common;
    setup = ./setup.sh;
    builder = build.shell;
    args = [ "-e" ../common/sub-builder.sh ];
  };
in

{ name
, sources
, binaries ? []
, includeDirs ? []
, libs
, dynamicLinker ? "ld"
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
abort "build.component is deprecated"
derivation ( args // rec {
  inherit name;
  component = result;
  system = result.system;
  outputs = [ "out" "src" ];
  builder = build.shell;
  args = [ "-e" (args.builder or ./default-builder.sh) ];

  objects =
    map (main: build.compileObject {
        inherit main ccFlags cxxFlags;
        localIncludePath = includeDirs;
      }) sources
    ++
    map (binary: build.transformBinary { inherit binary; }) binaries;
  objectSources = map (o: o.src) objects;

  phases = ( args.phases or [ "linkPhase" ] ) ++ [ "fixupPhase" ];

  inherit (build) cc cxx
    ldScriptDyn ldScriptSo;

  inherit (build.spec) ccMarch ldMarch;

  # Assemble linker options for static and dynamic linkage
  ldTextAddr = args.ldTextAddr or build.spec.ldTextAddr or "";

  sharedLibs = build.anyShared libs;

  ldScriptsStatic = args.ldScriptsStatic or build.spec.ldScriptsStatic or [ ../../../repos/base/src/platform/genode.ld ];
  ldScriptsDynamic = args.ldScriptsDynamic or [ build.ldScriptDyn ];

  ldScripts = if sharedLibs then ldScriptsDynamic else ldScriptsStatic;

  ldOpt = 
    (args.ldOpt or build.ldOpt)
    ++
    (if sharedLibs then
    # Add a list of symbols that shall always be added to the dynsym section
    [ "--dynamic-list=${../../../repos/os/src/platform/genode_dyn.dl}" ]
    else [ ]);

  cxxLinkOpt =
    (args.cxxLinkOpt or build.cxxLinkOpt)
    ++
    (map (x: "-Wl,${x}") ldOpt)
    ++
    [ "-nostdlib -Wl,-nostdlib"
      (if ldTextAddr != "" then "-Wl,-Ttext=${ldTextAddr}" else "")
      ccMarch
    ]
    ++
    (if sharedLibs then [
       "-Wl,--dynamic-linker=${dynamicLinker}.lib.so"
       "-Wl,--eh-frame-hdr"
     ] else [])
    ++
    (map (script: "-Wl,-T -Wl,${script}") ldScripts);

  verbose = builtins.getEnv "VERBOSE";
})