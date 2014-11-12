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

        for i in $propagatedIncludes
        do ln -s $i/* $include/ #*/
        done
    fi
  '';

  componentLinkPhase = ''
    echo -e "    LINK     $name"

    local _libs=$libs
    local libs=""

    for l in $_libs
    do findLibs $l libs
    done

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

in
rec {
  genodeEnv     = addSubMks genodeEnv';
  genodePortEnv = addSubMks genodeEnv;
  genodeEnvAdapters = import ./adapters.nix;
}
