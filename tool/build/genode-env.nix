/*
 * \brief  Standard Genode build environment
 * \author Emery Hemingway
 * \date   2014-08-23
 */

{ tool, spec, stdAttrs }:

with tool;

let

  shell = nixpkgs.bash + "/bin/sh";

  toolchain = nixpkgs.callPackage ../toolchain/precompiled {
    glibc = nixpkgs.glibc_multi;
  };

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
        [ tool.nixpkgsCross.binutilsCross toolchain ] ++
        (import ./common-path.nix { pkgs = nixpkgs; });
    } //
    stdAttrs //
    rec {
      meta = {
        description = "Genode build environment";
        maintainers = [ "Emery Hemingway <emery@vfemail.net>" ];
      };

      inherit system stdAttrs;
      # stdAttrs is a named attribute and a merged set.

      # Function to produce derivations.
      mk =
      attrs:

      let
        ldScriptsStatic =
          attrs.ldScriptsStatic or
          spec.ldScriptsStatic or
          [ ../../repos/base/src/platform/genode.ld ];

        ldScriptsDynamic =
          attrs.ldScriptsDynamic or
          spec.ldScriptsDynamic or
          [ ../../repos/base/src/platform/genode_dyn.ld ];

      in
      derivation (stdAttrs // attrs // {
        system = builtins.currentSystem;
        builder = shell;
        genodeEnv = result;
        args = [ "-e" ./default-builder.sh ];

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
