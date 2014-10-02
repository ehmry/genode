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
, includeDirs ? []
, libs
, dynamicLinker ? "ld"
, ... } @ args:

let

  anyShared = libs:
    if libs == [] then false else
       if (builtins.getAttr "shared" (builtins.head libs))
       then true else anyShared (builtins.tail libs);

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
derivation ( args // rec {
  inherit name;
  component = result;
  system = result.system;
  builder = shell;
  args = [ "-e" (args.builder or ./default-builder.sh) ];

  objects =
    map (source: compileObject { inherit source includeDirs ccFlags cxxFlags; }) sources
    ++
    map (binary: transformBinary { inherit binary; }) binaries;

  phases = ( args.phases or [ "linkPhase" ] );

  inherit (build) cc cxx
    ldScriptDyn ldScriptSo;

  inherit (build.spec) ccMarch ldMarch;

  # Assemble linker options for static and dynamic linkage
  ldTextAddr = args.ldTextAddr or build.spec.ldTextAddr or "";

  sharedLibs = anyShared libs;

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