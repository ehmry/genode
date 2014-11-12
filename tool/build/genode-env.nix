/*
 * \brief  Standard Genode build environment
 * \author Emery Hemingway
 * \date   2014-08-23
 */

{ system, tool }:

with tool;

let

  shell = nixpkgs.bash + "/bin/sh";

  ###################################################################
  # utility functions
  #

  compileObject =
  { main
  , localIncludes ? "auto"
  , localIncludePath ? []
  , ... } @ args:
  derivation (args // {
    name = "${baseNameOf (toString main)}-object";
    system = builtins.currentSystem;
    outputs = [ "out" "src" ];
    builder = shell;
    args = [ "-e" ./compile-object.sh ];
    inherit main common cc cxx nativeIncludePaths verbose;

    localIncludes =
      if localIncludes == "auto" then
        map (x: [ x.key x.relative ]) (builtins.genericClosure {
          startSet = [ { key = main; relative = baseNameOf (toString main); } ];
          operator =
            { key, ... }:
            let
              includes = import (findIncludes { main = key; });
              includesFound = nixpkgs.lib.concatMap (fn: findFile fn localIncludePath) includes;
            in includesFound;
        })
      else
        localIncludes;
  });

  transformBinary =
  { binary }:
  derivation {
    name =
      if builtins.typeOf binary == "path"
      then "${baseNameOf (toString binary)}-binary"
      else "binary";
    system = builtins.currentSystem;
    genodeEnv = result;
    inherit binary;
    inherit (stdAttrs) as asOpt;
    builder = shell;
    args = [ "-e" ./transform-binary.sh ];
  };

  findSrc = fn: pathSet:
    let
      path =
        pathSet."${fn}" or
        pathSet."*" or "";
      fn' = path + "/${fn}";
    in
    assert false;
    if builtins.typeOf fn == "list" then
      (assert (builtins.length fn == 2); fn)
    else
    if builtins.typeOf fn == "path" then
      [ fn (baseNameOf (toString fn)) ]
    else
      if builtins.pathExists fn' then [ fn' fn ]
      else abort "no search path for ${fn}";

  spec =
    if system == "x86_32-linux" then import ../../specs/x86_32-linux.nix else
    if system == "x86_64-linux" then import ../../specs/x86_64-linux.nix else
    if system == "x86_32-nova"  then import ../../specs/x86_32-nova.nix  else
    if system == "x86_64-nova"  then import ../../specs/x86_64-nova.nix  else
    abort "unknown system type ${system}";

  toolchain = nixpkgs.callPackage ../toolchain/precompiled {
    glibc = nixpkgs.glibc_multi;
  };

  devPrefix = "genode-${spec.platform}-";

  # attributes that should always be present
  # and accessible through genodeEnv."..."
  # TODO split into compile, component, library?
  stdAttrs = rec
    { verbose = 1;

      cc      = "${devPrefix}gcc";
      cxx     = "${devPrefix}g++";
      ld      = "${devPrefix}ld";
      as      = "${devPrefix}as";
      ar      = "${devPrefix}ar";
      nm      = "${devPrefix}nm";
      objcopy = "${devPrefix}objcopy";
      ranlib  = "${devPrefix}ranlib";
      strip   = "${devPrefix}strip";

      ccOLevel = "-O2";
      ccWarn   = "-Wall";
      ccCxxOpt = [ "-std=gnu++11" ];

      ccOpt =
        [ # Always compile with '-ffunction-sections' to enable
          # the use of the linker option '-gc-sections'
          "-ffunction-sections"

          # Prevent the compiler from optimizations 
          # related to strict aliasing
          "-fno-strict-aliasing"

          # Do not compile with standard includes per default.
          "-nostdinc"

          "-g"
          ]
          ++ spec.ccMarch ++ spec.ccOpt;

      ccCOpt = ccOpt;

      ldMarch = spec.ldMarch or [];

      ldOpt = ldMarch ++ [ "-gc-sections" ];

      asOpt = spec.asMarch;

      cxxLinkOpt = [ "-nostdlib -Wl,-nostdlib"] ++ spec.ccMarch;

      nativeIncludePaths =
        [ "${toolchain}/lib/gcc/${spec.target}/${toolchain.version}/include"
        ];
    };

  # This is pretty much the same thing as genodeEnv/setup, deprecated.
  common = import ./common { inherit nixpkgs toolchain; };

  # The genodeEnv that we are producing.
  result =

    derivation {
      name = "genodeEnv";
      system = builtins.currentSystem;
      builder = shell;
      args = [ "-e" ./setup-builder.sh ];

      setup = ./setup.sh;
      inherit shell toolchain;
      initialPath =
        (import ./common-path.nix { pkgs = nixpkgs; })
        ++ [ toolchain ];
    } //
    stdAttrs //
    rec {
      meta = {
        description = "Genode build environment";
        maintainers = [ "Emery Hemingway <emery@vfemail.net>" ];
      };

      inherit system;

      # Function to produce derivations.
      mk =
      attrs:
      #assert builtins.hasAttr "sources" attrs;
      assert !builtins.hasAttr "srcS" attrs;
      assert !builtins.hasAttr "srcC" attrs;
      assert !builtins.hasAttr "srcCC" attrs;
      assert !builtins.hasAttr "srcSh" attrs;
      #assert (builtins.hasAttr "sourceSh" attrs) -> (builtins.hasAttr "sourceRoot");
      let
        fixName = s: replaceInString "." "_" s;

        localIncludesFun =
        path: absRel:
        let
          abs = builtins.elemAt absRel 0;
          key = fixName (baseNameOf (toString abs));

          # Search the directory of the source file first.
          path' = [ (builtins.dirOf (toString abs)) ] ++ path;
        in
        { name = "local_includes_"+key;
          value = builtins.tail (findLocalIncludes abs path');
        };

        # Generate a set of { incude_file_cc = [ ... ] }.
        localIncludesSet = builtins.listToAttrs (
          map
            (localIncludesFun attrs.localIncludes or [])
            attrs.sources or []
        );

        ldScriptsStatic =
          attrs.ldScriptsStatic or
          spec.ldScriptsStatic or
          [ ../../../repos/base/src/platform/genode.ld ];

        ldScriptsDynamic =
          attrs.ldScriptsDynamic or
          spec.ldScriptsDynamic or
          [ ../../repos/os/src/platform/genode_dyn.ld ];

      in
      derivation (stdAttrs // attrs // localIncludesSet // {
        system = builtins.currentSystem;
        builder = shell;
        genodeEnv = result;
        args = [ "-e" ./default-builder.sh ];

        ## If a library has an "include" output, add it
        ## to the list of system include paths.
        systemIncludes = (attrs.systemIncludes or [])
          ++ map (l: l.include or "") (attrs.libs or []);

        # Transfrom binaries as individual derivations,
        # I may do the same thing with sources some day.
        # It is unclear if the time saved in partial recompilation is
        # worth the time evaluating rarely changed inputs.
        extraObjects = map
          (binary: transformBinary { inherit binary; })
          attrs.binaries or [];
                  
        ldScripts =
          if anyShared (attrs.libs or [])
            then ldScriptsDynamic
            else ldScriptsStatic;
      });

       inherit spec;

       # Utility flags to test the type of platform.
       is32Bit  = spec.bits == 32;
       is64Bit  = spec.bits == 64;
       isArm = spec.platform == "arm";
       isx86 = spec.platform == "x86";
       isx86_32 = isx86 && is32Bit;
       isx86_64 = isx86 && is64Bit;

       isLinux = spec.kernel == "linux";
       isNova  = spec.kernel == "nova";
       isNOVA  = isNova;

       inherit tool;

       inherit (tool) fromDir fromPath fromPaths;
     };

in result

/*

I need to construct a set of each source file's dependencies, like this:

{
  "ipc/ipc.hc" = [ [ "/home/emery/repo/genode/repos/base-linux/src/base/ipc/socket_descriptor_registry.h socket_descriptor_registry.h" ] ];
}

the way to do it is to map sources with a fuction that returns { name = (source); value = (includes); },
then do a listToAttrs

this is good to do now, because I'll want to do the same thing if I do per file object derivation


*/
