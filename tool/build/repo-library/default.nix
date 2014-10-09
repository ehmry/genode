{ build }:

let
  result = derivation
    { name = "build-repo-library";
      system = builtins.currentSystem;

      inherit (build) common;
      setup = ./setup.sh;
      builder = build.shell;
      args = [ "-e" ../common/sub-builder.sh ];
    };
in

{ name
, shared ? false
, src, sources
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
  repoLibrary = result;
  system = result.system;

  inherit libs entryPoint;

  inherit (build) cc cxx ar ld ldOpt objcopy verbose;
  inherit (build.spec) ccMarch;

  inherit ccFlags cxxFlags ldFlags;

  ldScriptSo = args.ldScriptSo or ../../../repos/os/src/platform/genode_rel.ld;

  builder = build.shell;
  args = [ "-e" (args.builder or ./default-builder.sh)];

  phases = args.phases or
    [ "compilePhase" ]
    ++
    ( if shared then [ "mergeSharedPhase" ]
      else [ "mergeStaticPhase" ])
    ++ [ "fixupPhase" ];

  # not used by the builder but by further dependencies
  # so actually this belongs in a higher set that wraps
  # the derivation
  inherit propagatedIncludes;
})