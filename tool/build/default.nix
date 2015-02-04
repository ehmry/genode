/*
 * \brief  Build environment
 * \author Emery Hemingway
 * \date   2014-08-23
 */

{ spec, tool }:

with tool;

let
  toolchain = nixpkgs.callPackage ../toolchain/precompiled { };

  # TODO deduplicate with ../build/genode-env.nix
  # attributes that should always be present
  stdAttrs = rec
    { verbose = 1;

      cc      = "${spec.crossDevPrefix}gcc";
      cxx     = "${spec.crossDevPrefix}g++";
      ld      = "${spec.crossDevPrefix}ld";

      ar      = "${spec.target}-ar";
      as      = "${spec.target}-as";
      nm      = "${spec.target}-nm";
      objcopy = "${spec.target}-objcopy";
      ranlib  = "${spec.target}-ranlib";
      strip   = "${spec.target}-strip";

      ccMarch = spec.ccMarch or [];
      ldMarch = spec.ldMarch or [];
      asMarch = spec.ldMarch or [];

      ccWarn   = "-Wall";

      ccFlags =
        (spec.ccFlags or []) ++ spec.ccMarch ++
        [ ccWarn

          # Always compile with '-ffunction-sections' to enable
          # the use of the linker option '-gc-sections'
          "-ffunction-sections"

          # Prevent the compiler from optimizations
          # related to strict aliasing
          "-fno-strict-aliasing"

          # Do not compile with standard includes per default.
          "-nostdinc"

          "-g"
        ];

      cxxFlags = [ "-std=gnu++11" ];

      ccCFlags = ccFlags;

      ldFlags = ldMarch ++ [ "-gc-sections" ];

      asFlags = spec.asMarch;

      cxxLinkFlags = [ "-nostdlib -Wl,-nostdlib"] ++ spec.ccMarch;

      nativeIncludes =
        [ "${toolchain}/lib/gcc/${spec.target}/${toolchain.version}/include" ];
    };

  propagateLibAttrs = attrs:
    mergeSets (
      (map (x: x.propagate or {}) (findLibraries attrs.libs or [])) ++
      [ attrs ]
    );

  oLevel = "-O2";

in
rec {
  inherit toolchain;

  genodeEnv = import ./genode-env.nix {
     inherit tool spec stdAttrs;
  };
  genodeEnvAdapters = import ./adapters.nix;

  ## TODO
  # Deduplicate the following functions.
  # It shouldn't be hard, the scripts can
  # be broken apart into files to be sourced
  # by other scripts.

  compiles =
  { src , PIC ? true, optimization ? oLevel, ... } @ args:
  let
    mappings = includesOfFiles [ src ] args.includes or [];
  in
  shellDerivation (
    { inherit (stdAttrs) cc ccFlags;
      inherit genodeEnv PIC optimization;
    } //
    args //
    {
      name = dropSuffix ".s" (baseNameOf (toString src)) + ".o";
      script = ./compile-s.sh;
      relative = builtins.attrNames  mappings;
      absolute = builtins.attrValues mappings;
    }
  );

  compileS =
  { src, PIC ? true, optimization ? oLevel, ... } @ args:
  let
    mappings = includesOfFiles [ src ] args.includes or [];
  in
  shellDerivation (
    { inherit (stdAttrs) cc ccFlags;
      inherit genodeEnv PIC optimization;
    } //
    args //
    {
      name = dropSuffix ".S" (baseNameOf (toString src)) + ".o";
      script = ./compile-S.sh;
      relative = builtins.attrNames  mappings;
      absolute = builtins.attrValues mappings;
    }
  );

  compileC =
  { src, PIC ? true, optimization ? oLevel, ... } @ args:
  let
    args' = propagateLibAttrs args;
    mappings = includesOfFiles [ src ] args'.includes or [];
  in
  shellDerivation (removeAttrs args' [ "libs" "runtime" ] //
    { inherit (stdAttrs) cc ccFlags;
      inherit PIC optimization genodeEnv;
      name = dropSuffix ".c" (baseNameOf (toString src)) + ".o";
      script = ./compile-c.sh;
      relative = builtins.attrNames  mappings;
      absolute = builtins.attrValues mappings;
    }
  );

  compileCC =
  { src, PIC ? true, optimization ? oLevel, ... } @ args:
  let
    args' = propagateLibAttrs args;
    mappings = includesOfFiles ([ src ] ++ args'.headers or []) args'.includes or [];
  in
  shellDerivation (removeAttrs args' [ "libs" "runtime" ] //
    { inherit (stdAttrs) cxx ccFlags cxxFlags;
      inherit PIC optimization genodeEnv;
      name = dropSuffix ".cc" (baseNameOf (toString src)) + ".o";
      script = ./compile-cc.sh;

      relative = builtins.attrNames  mappings;
      absolute = builtins.attrValues mappings;
      # If there are no unresolved relative includes, externalIncludes could be dropped.
      externalIncludes = args'.externalIncludes or [] ++ stdAttrs.nativeIncludes;
    }
  );

  transformBinary =
  binary:
  let
    s = baseNameOf (toString binary);
    fixName = s:
      let
        l = builtins.stringLength s;
        c = builtins.substring 0 1 s;
      in
      if l == 0 then "" else
        (if c == "." || c == "-" then "_" else c) +
        fixName (builtins.substring 1 l s);
    symbolName = "_binary_" + fixName s;
  in
  shellDerivation {
    name =
      if builtins.typeOf binary == "path"
      then symbolName + ".o"
      else "binary";
    script = ./transform-binary.sh;
    inherit genodeEnv binary symbolName;
    inherit (stdAttrs) as asFlags;
  };

  # Compile objects from a port derivation.
  compileCRepo =
  { name ? "objects"
  , sources
  , PIC ? true
  , optimization ? "-O2"
  , ... } @ args:
  let
    args' = propagateLibAttrs args;
    sources' = if builtins.typeOf sources == "list" then sources else [ sources ];
    mappings = includesOfFiles (sources' ++ args'.headers or []) args'.includes or [];
  in
  shellDerivation (removeAttrs args' [ "runtime" "libs" ] //
    { inherit name PIC optimization genodeEnv;
      inherit (stdAttrs) cc ccFlags;
      script = ./compile-c-external.sh;
      relative = builtins.attrNames  mappings;
      absolute = builtins.attrValues mappings;
      externalIncludes = args'.externalIncludes or [] ++ stdAttrs.nativeIncludes;
    }
  );

  # Compile objects from a port derivation.
  compileCCRepo =
  { name ? "objects"
  , sources
  , PIC ? true
  , optimization ? "-O2"
  , ... } @ args:
  let
    args' = propagateLibAttrs args;
    sources' = if builtins.typeOf sources == "list" then sources else [ sources ];
    mappings = includesOfFiles (sources' ++ args'.headers or []) args'.includes or [];
in
  shellDerivation (removeAttrs args' [ "runtime" "libs" ] //
    { inherit name PIC optimization genodeEnv;
      inherit (stdAttrs) cxx ccFlags cxxFlags;
      script = ./compile-cc-external.sh;
      relative = builtins.attrNames  mappings;
      absolute = builtins.attrValues mappings;
      externalIncludes = args'.externalIncludes or [] ++ stdAttrs.nativeIncludes;
    }
  );

  # Link together a static library.
  linkStaticLibrary =
  { libs ? [], propagate ? {}, ... } @ args:
  shellDerivation (removeAttrs args ["propagate"] // {
    script = ./link-static-library.sh;
    inherit genodeEnv;
    inherit (stdAttrs) ar ld ldFlags cxx objcopy;
    libs = filterFakeLibs libs;
  }) // { inherit propagate; shared = false; };

  # Link together a shared library.
  # This function must be preloaded with an ldso-startup library.
  linkSharedLibrary =
  { ldso-startup }:
  { ldScriptSo ? ../../repos/base/src/platform/genode_rel.ld
  , entryPoint ? "0x0"
  , libs ? []
  , propagate ? {}
  , ... } @ args:
  shellDerivation (removeAttrs args ["propagate"] // {
    script = ./link-shared-library.sh;
    libs = [ ldso-startup ] ++ findLinkLibraries libs;
    inherit genodeEnv ldScriptSo entryPoint;
     inherit (stdAttrs) ld ldFlags cc ccMarch;
  }) // { inherit propagate; shared = true; };

  # Link together a component with static libraries.
  linkStaticComponent =
  { ldTextAddr ? spec.ldTextAddr
  , ldScripts ? [ ../../repos/base/src/platform/genode.ld ]
  , libs ? []
  , ... } @ args:
  let args' = propagateLibAttrs args; in
  shellDerivation (removeAttrs args' [ "propagate" "runtime" ] // {
    script = ./link-component.sh;
    inherit genodeEnv ldTextAddr ldScripts;
    inherit (stdAttrs) ld ldFlags cxx cxxLinkFlags cc ccMarch;
    libs = findLinkLibraries libs;
  }) // { runtime = args'.runtime or { }; };

  # Link together a component with shared libraries.
  linkDynamicComponent =
  { dynamicLinker
  ,  dynDl ? ../../repos/base/src/platform/genode_dyn.dl
  , ldTextAddr ? spec.ldTextAddr
  , ldScripts ? [ ../../repos/base/src/platform/genode_dyn.ld ]
  , libs ? []
  , ... } @ args:
  let args' = propagateLibAttrs args; in
  shellDerivation (removeAttrs args' [ "propagate" "runtime" ] // {
    script = ./link-component.sh;
    inherit genodeEnv dynDl ldTextAddr ldScripts;
    inherit (stdAttrs) ld ldFlags cxx cxxLinkFlags cc ccMarch;
    libs = (findLinkLibraries libs) ++ [ dynamicLinker ];
    dynamicLinker = "${dynamicLinker.name}.lib.so";
  }) // { runtime = args'.runtime or { } // { libs = findRuntimeLibraries libs; }; };

}
