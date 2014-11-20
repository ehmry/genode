/*
 * \brief  Build environment
 * \author Emery Hemingway
 * \date   2014-08-23
 */

{ system, tool }:

with tool;

let

  # Linker script for dynamically linked programs
  ldScriptDyn = ../../repos/os/src/platform/genode_dyn.ld;

  # Linker script for shared libraries
  ldScriptSo  = ../../repos/os/src/platform/genode_rel.ld;

  staticLibraryLinkPhase = ''
    objects=$(sortWords $objects)

    echo -e "    MERGE    $name"
    mkdir -p $out
    VERBOSE $ar -rc $out/$name.lib.a $objects

    if [ -n "$libs" ]; then
        libs=$(sortDerivations $libs)
        mkdir -p "$out/nix-support"
        echo "$libs" > "$out/nix-support/propagated-libraries"
    fi
  '';

  sharedLibraryLinkPhase = ''
    echo -e "    MERGE    $@"

    objects=$(sortWords $objects)
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

    for o in $ldOpt
    do cxxLinkOpt="$cxxLinkOpt -Wl,$o"
    done

    VERBOSE $cxx $cxxLinkOpt \
	-Wl,--whole-archive -Wl,--start-group \
        $objects $libs \
	-Wl,--end-group -Wl,--no-whole-archive \
        $($cc $ccMarch -print-libgcc-file-name) \
        -o $out/$name
  '';

  componentFixupPhase = " ";

  genodeEnv'   = import ./genode-env.nix { inherit system tool; };
  genodePortEnv' = genodeEnv // {
    mk =
    { portSrc, ... } @ attrs:
    genodeEnv'.mk { gatherPhase = "cd \"$portSrc\""; };
  };

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

      ccMarch = spec.ccMarch or [];
      ldMarch = spec.ldMarch or [];
      asMarch = spec.ldMarch or [];

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
          ++ spec.ccMarch ++ (spec.ccOpt or []);

      ccCOpt = ccOpt;

      ldOpt = ldMarch ++ [ "-gc-sections" ];

      asOpt = spec.asMarch;

      cxxLinkOpt = spec.ccMarch ++ [ "-nostdlib -Wl,-nostdlib"];

      nativeIncludePaths =
        [ "${toolchain}/lib/gcc/${spec.target}/${toolchain.version}/include"
        ];
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
  derivation (stdAttrs // args // {
    name = "${baseNameOf (toString src)}-object";
    system = builtins.currentSystem;
    builder = shell;
    args = [ "-e" ./compile-s.sh ];
    main = src;

    inherit genodeEnv;

    localIncludes = findLocalIncludes src localIncludes;
  });

  compileC =
  { src
  , localIncludes ? []
  , ... } @ args:
  derivation (stdAttrs // args // {
    name = "${baseNameOf (toString src)}-object";
    system = builtins.currentSystem;
    builder = shell;
    args = [ "-e" ./compile-c.sh ];
    main = src;

    inherit genodeEnv;

    localIncludes = findLocalIncludes src localIncludes;
  });

  compileCC =
  { src
  , localIncludes ? []
  , ... } @ args:
  derivation (stdAttrs // args // {
    name = "${baseNameOf (toString src)}-object";
    system = builtins.currentSystem;
    builder = shell;
    args = [ "-e" ./compile-cc.sh ];
    main = src;

    inherit genodeEnv;

    localIncludes = findLocalIncludes src localIncludes;
  });

}
