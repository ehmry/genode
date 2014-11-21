/*
 * \brief  Build environment
 * \author Emery Hemingway
 * \date   2014-08-23
 */

{ system, tool }:

with tool;

let
  devPrefix = "genode-${spec.platform}-";

  # TODO deduplicate with ../build/genode-env.nix
  # attributes that should always be present
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

      ccCOpt = ccFlags;

      ldOpt = ldMarch ++ [ "-gc-sections" ];

      asOpt = spec.asMarch;

      cxxLinkOpt =
        (map (o: "-Wl,"+o) ldOpt) ++
        spec.ccMarch ++ [ "-nostdlib -Wl,-nostdlib"];

      nativeIncludePaths =
        [ "${toolchain}/lib/gcc/${spec.target}/${toolchain.version}/include"
        ];
    };


  # Linker script for dynamically linked programs
  ldScriptDyn = ../../repos/os/src/platform/genode_dyn.ld;

  # Linker script for shared libraries
  ldScriptSo  = ../../repos/os/src/platform/genode_rel.ld;

  staticLibraryLinkPhase = ''
    echo -e "    MERGE    $name"

    for o in $externalObjects
    do objects="$objects $o/*.o"
    done

    mkdir -p $out
    VERBOSE $ar -rc $out/$name.lib.a $objects

    if [ -n "$libs" ]; then
        libs=$(sortDerivations $libs)
        mkdir -p "$out/nix-support"
        echo "$libs" > "$out/nix-support/propagated-libraries"
    fi
  '';

  sharedLibraryLinkPhase = ''
    echo -e "    MERGE    $name"

    for o in $externalObjects
    do objects="$objects $o/*.o"
    done

    local _libs=$(sortDerivations $libs)
    local libs=""

    for l in $_libs
    do libs="$libs $l/*.a $l/*.so"
    done

    mkdir -p $out

    VERBOSE $ld -o $out/$name.lib.so -shared --eh-frame-hdr \
        $ldOpt $ldFlags \
	-T $ldScriptSo \
        --entry=$entryPoint \
	--whole-archive \
	--start-group \
        $libs $objects \
	--end-group \
	--no-whole-archive \
        $($cc $ccMarch -print-libgcc-file-name)
  '';


  ## could propagatedIncludes replace systemIncludes?

  libraryFixupPhase = ''
    if [ -n "$libs" ]; then
        mkdir -p "$out/nix-support"
        echo "$libs" > "$out/nix-support/propagated-libraries"
    fi

    if [ -n "$propagatedIncludes" ]; then
        mkdir -p $include

        for i in $propagatedIncludes; do
            cp -ru --no-preserve=mode $i/* $include
        done
    fi
  '';
  #*/

  componentLinkPhase = ''
    echo -e "    LINK     $name"

    for o in $externalObjects
    do objects="$objects $o/*.o"
    done

    objects=$(sortWords $objects)

    local _libs=$libs
    local libs=""

    for l in $_libs
    do findLibs $l libs
    done

    libs=$(sortDerivations $libs)

    mkdir -p $out

    [[ "$ldTextAddr" ]] && \
        cxxLinkOpt="$cxxLinkOpt -Wl,-Ttext=$ldTextAddr"

    for s in $ldScripts
    do cxxLinkOpt="$cxxLinkOpt -Wl,-T -Wl,$s"
    done

    VERBOSE $cxx $cxxLinkOpt \
	-Wl,--whole-archive -Wl,--start-group \
        $objects $libs \
	-Wl,--end-group -Wl,--no-whole-archive \
        $($cc $ccMarch -print-libgcc-file-name) \
        -o $out/$name
  '';

  componentFixupPhase = " ";


  genodeEnv' = import ./genode-env.nix {
     inherit system tool spec stdAttrs;
  }; # wtf is this?

  addSubMks = env: env // {

    mkLibrary = attrs: env.mk (
      { fixupPhase = libraryFixupPhase;
        outputs =
          if builtins.hasAttr "propagatedIncludes" attrs then
            [ "out" "include" ]
          else [ "out" ];
      } //
      (if attrs.shared or false then
         { mergePhase = sharedLibraryLinkPhase;
           entryPoint = attrs.entryPoint or "0x0";
           inherit ldScriptSo;
         }
       else
         { mergePhase = staticLibraryLinkPhase; }
      )
      // attrs
    );

    mkComponent = attrs: env.mk (
      let anyShared' = anyShared (attrs.libs or []); in
      { mergePhase  = componentLinkPhase;
        fixupPhase = componentFixupPhase;
        ldTextAddr = attrs.ldTextAddr or env.spec.ldTextAddr or "";
        ldOpt = attrs.ldOpt or env.ldOpt ++ (
          if anyShared' then
            # Add a list of symbols that shall
            # always be added to the dynsym section
            [ "--dynamic-list=${../../repos/os/src/platform/genode_dyn.dl}"
              # Assume that 'dynamicLinker' has been added to attrs.
              "--dynamic-linker=${attrs.dynamicLinker}"
              "--eh-frame-hdr"
            ]
          else []
        );
      } // attrs
    );

  };

  spec =
    if system == "x86_32-linux" then import ../../specs/x86_32-linux.nix else
    if system == "x86_64-linux" then import ../../specs/x86_64-linux.nix else
    if system == "x86_32-nova"  then import ../../specs/x86_32-nova.nix  else
    if system == "x86_64-nova"  then import ../../specs/x86_64-nova.nix  else
    abort "unknown system type ${system}";

  toolchain = nixpkgs.callPackage ../toolchain/precompiled {
    glibc = nixpkgs.glibc_multi;
  };

in
rec {
  genodeEnv     = addSubMks genodeEnv';
  genodePortEnv = addSubMks genodeEnv;
  genodeEnvAdapters = import ./adapters.nix;

  compileS =
  { src
  , localIncludes ? []
  , ... } @ args:
  derivation (
    { inherit (stdAttrs) cc ccFlags nativeIncludePaths;
      inherit genodeEnv;
    } //
    args //
    {
      name = "${baseNameOf (toString src)}-object";
      system = builtins.currentSystem;
      builder = shell;
      args = [ "-e" ./compile-s.sh ];

      localIncludes = findLocalIncludes src localIncludes;
    }
  );

  compileC =
  { src
  , localIncludes ? []
  , ... } @ args:
  derivation (
    { inherit (stdAttrs) cc ccFlags nativeIncludePaths;
      inherit genodeEnv;
    } //
    args //
    {
      name = dropSuffix ".c" (baseNameOf (toString src)) + ".o";
      system = builtins.currentSystem;
      builder = shell;
      args = [ "-e" ./compile-c.sh ];

      localIncludes = findLocalIncludes src localIncludes;
    }
  );

  compileCC =
  { src
  , localIncludes ? []
  , ... } @ args:
  derivation (
    { inherit (stdAttrs) cxx ccFlags cxxFlags nativeIncludePaths;
      inherit genodeEnv;
    } //
    args //
    {
      name = dropSuffix ".cc" (baseNameOf (toString src)) + ".o";
      system = builtins.currentSystem;
      builder = shell;
      args = [ "-e" ./compile-cc.sh ];

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
    symbolName = fixName s;
  in
  derivation {
    name =
      if builtins.typeOf binary == "path"
      then symbolName + ".o"
      else "binary";
    system = builtins.currentSystem;
    builder = tool.shell;
    args = [ "-e" ./transform-binary.sh ];

    inherit genodeEnv binary symbolName;
    inherit (stdAttrs) as asOpt;
  };

}
