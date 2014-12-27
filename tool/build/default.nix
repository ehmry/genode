/*
 * \brief  Build environment
 * \author Emery Hemingway
 * \date   2014-08-23
 */

{ spec, tool }:

with tool;

let
  toolchain = nixpkgs.callPackage ../toolchain/precompiled {};

  devPrefix = "genode-${spec.platform}-";

  # TODO deduplicate with ../build/genode-env.nix
  # attributes that should always be present
  stdAttrs = rec
    { verbose = 1;

      cc      = "${devPrefix}gcc";
      cxx     = "${devPrefix}g++";
      ld      = "${devPrefix}ld";

      ar      = "${spec.target}-ar";
      as      = "${spec.target}-as";
      nm      = "${spec.target}-nm";
      objcopy = "${spec.target}-objcopy";
      ranlib  = "${spec.target}-ranlib";
      strip   = "${spec.target}-strip";

      ccMarch = spec.ccMarch or [];
      ldMarch = spec.ldMarch or [];
      asMarch = spec.ldMarch or [];

      ccOLevel = "-O2";
      ccWarn   = "-Wall";

      ccFlags =
        (spec.ccFlags or []) ++ spec.ccMarch ++
        [ ccOLevel ccWarn

          # Always compile with '-ffunction-sections' to enable
          # the use of the linker option '-gc-sections'
          "-ffunction-sections"

          # Prevent the compiler from optimizations
          # related to strict aliasing
          "-fno-strict-aliasing"

          # Do not compile with standard includes per default.
          "-nostdinc"

          # It shouldn't be too difficult to make a function to
          # turn this back on for specific components.
          #"-g"
        ];

      cxxFlags = [ "-std=gnu++11" ];

      ccCFlags = ccFlags;

      ldFlags = ldMarch ++ [ "-gc-sections" ];

      asFlags = spec.asMarch;

      cxxLinkFlags = [ "-nostdlib -Wl,-nostdlib"] ++ spec.ccMarch;

      nativeIncludes =
        [ "${toolchain}/lib/gcc/${spec.target}/${toolchain.version}/include"
        ];
    };

  propagateCompileArgs = args:
    let
      libs = findLibraries args.libs or [];
    in
    (removeAttrs args [ "libs" ]) //
    { extraFlags =
        args.extraFlags or [] ++
        (propagate "propagatedFlags" libs);

      systemIncludes =
        args.systemIncludes or [] ++
        (propagate "propagatedIncludes" libs) ++
        stdAttrs.nativeIncludes;
    };

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
  { src
  , localIncludes ? []
  , ... } @ args:
  shellDerivation (
    { inherit (stdAttrs) cc ccFlags;
      inherit genodeEnv;
    } //
    args //
    {
      name = dropSuffix ".s" (baseNameOf (toString src)) + ".o";
      script = ./compile-s.sh;
      localIncludes = findLocalIncludes src localIncludes;
    }
  );

  compileS =
  { src
  , localIncludes ? []
  , ... } @ args:
  shellDerivation (
    { inherit (stdAttrs) cc ccFlags;
      inherit genodeEnv;
    } //
    args //
    {
      name = dropSuffix ".S" (baseNameOf (toString src)) + ".o";
      script = ./compile-S.sh;
      localIncludes = findLocalIncludes src localIncludes;
    }
  );

  compileC =
  { src
  , localIncludes ? []
  , PIC ? true
  , ... } @ args:
  shellDerivation ( (propagateCompileArgs args) //
    { inherit (stdAttrs) cc ccFlags;
      inherit PIC genodeEnv;
      name = dropSuffix ".c" (baseNameOf (toString src)) + ".o";
      script = ./compile-c.sh;
      localIncludes = findLocalIncludes src localIncludes;
    }
  );

  compileCC =
  { src
  , localIncludes ? []
  , PIC ? true
  , ... } @ args:
  shellDerivation ( (propagateCompileArgs args) //
    { inherit (stdAttrs) cxx ccFlags cxxFlags;
      inherit PIC genodeEnv;
      name = dropSuffix ".cc" (baseNameOf (toString src)) + ".o";
      script = ./compile-cc.sh;
      localIncludes = findLocalIncludes src localIncludes;
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
  , ... } @ args:
  shellDerivation ( (propagateCompileArgs args) //
    { inherit name PIC genodeEnv;
      inherit (stdAttrs) cc ccFlags;
      script = ./compile-c-port.sh;
    }
  );

  # Compile objects from a port derivation.
  compileCCRepo =
  { name ? "objects"
  , sources
  , PIC ? true
  , ... } @ args:
  shellDerivation ( (propagateCompileArgs args) //
    { inherit name PIC genodeEnv;
      inherit (stdAttrs) cxx ccFlags cxxFlags;
      script = ./compile-cc-port.sh;
    }
  );

  # Link together a static library.
  linkStaticLibrary =
  { libs ? [], ... } @ args:
  shellDerivation ( args // {
    script = ./link-static-library.sh;
    inherit genodeEnv;
    inherit (stdAttrs) ar ld ldFlags cxx objcopy;
    libs = filterFakeLibs libs;
  }) // { shared = false; };

  # Link together a shared library.
  # This function must be preloaded with an ldso-startup library.
  linkSharedLibrary =
  { ldso-startup }:
  { ldScriptSo ? ../../repos/base/src/platform/genode_rel.ld
  , entryPoint ? "0x0"
  , libs ? []
  , ... } @ args:
  shellDerivation ( args // {
    script = ./link-shared-library.sh;
    libs = [ ldso-startup ] ++ findLinkLibraries libs;
    inherit genodeEnv ldScriptSo entryPoint;
    inherit (stdAttrs) ld ldFlags cc ccMarch;
  }) // { shared = true; };

  # Link together a component with static libraries.
  linkStaticComponent =
  { ldTextAddr ? spec.ldTextAddr
  , ldScripts ? [ ../../repos/base/src/platform/genode.ld ]
  , libs ? []
  , ... } @ args:
  let
    libs' = libs;
  in
  shellDerivation ( args // {
    script = ./link-static-component.sh;
    inherit genodeEnv ldTextAddr ldScripts;
    inherit (stdAttrs) ld ldFlags cxx cxxLinkFlags cc ccMarch;
    libs = findLinkLibraries libs';
  });

  # Link together a component with shared libraries.
  linkDynamicComponent =
  { dynamicLinker
  ,  dynDl ? ../../repos/base/src/platform/genode_dyn.dl
  , ldTextAddr ? spec.ldTextAddr
  , ldScripts ? [ ../../repos/base/src/platform/genode_dyn.ld ]
  , libs ? []
  , ... } @ args:
  let
    libs' = libs;
  in
  shellDerivation ( args // {
    script = ./link-dynamic-component.sh;
    inherit genodeEnv dynDl ldTextAddr dynamicLinker ldScripts;
    inherit (stdAttrs) ld ldFlags cxx cxxLinkFlags cc ccMarch;
    libs = (findLinkLibraries libs') ++ [ dynamicLinker ];
  }) // { libs = findRuntimeLibraries libs'; };

}
