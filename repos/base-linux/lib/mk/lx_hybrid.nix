{ linkStaticLibrary, compileCC, baseDir, repoDir, mergeSets, nixpkgs
, genodeEnv, env, base-common, syscall }:

linkStaticLibrary (mergeSets [
  { name = "lx_hybrid";
    libs = [ base-common ];
    objects = map
      (src: compileCC {
        inherit src;
        libs = [ syscall ];
      })
      [ (repoDir + "/src/platform/lx_hybrid.cc")
        (baseDir + "/src/base/cxx/new_delete.cc")
      ];

    propagate =
      { inherit (nixpkgs) gcc;
        buildInputs = [ nixpkgs.gcc nixpkgs.pkgconfig ];
        preLink = builtins.readFile ../lx_hybrid_preStart.sh;
        systemIncludes = [ "${nixpkgs.gcc.libc}/include" ];
      };
  }
  (import ./base.inc.nix { inherit genodeEnv compileCC baseDir repoDir base-common syscall env; })
])
