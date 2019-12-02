{ localSystem ? "x86_64-linux", crossSystem ? "x86_64-genode", self ? { }, ...
}@args:

let
  pinnedNixpkgs = import (builtins.fetchGit {
    url = "https://gitea.c3d2.de/ehmry/nixpkgs.git";
    ref = "genode";
  });

  nixpkgs =
    args.nixpkgs or (pinnedNixpkgs { inherit localSystem crossSystem; });

  inherit (nixpkgs) stdenv buildPackages fetchgit llvmPackages;

  src = self.outPath or ./.;
  version = self.lastModified or "unstable";

  inherit (stdenv) lib targetPlatform;
  specs = with targetPlatform;
    [ ] ++ lib.optional is32bit "32bit" ++ lib.optional is64bit "64bit"
    ++ lib.optional isAarch32 "arm" ++ lib.optional isAarch64 "arm_64"
    ++ lib.optional isRiscV "riscv" ++ lib.optional isx86 "x86"
    ++ lib.optional isx86_32 "x86_32" ++ lib.optional isx86_64 "x86_64";

  buildRepo = { repo, repoInputs }:
    let
      tupArch = with stdenv.targetPlatform;

        if isAarch32 then
          "arm"
        else

        if isAarch64 then
          "arm64"
        else

        if isx86_32 then
          "i386"
        else

        if isx86_64 then
          "x86_64"
        else

          abort "unhandled targetPlatform";

      toTupConfig = attrs:
        with builtins;
        let op = config: name: "${config}CONFIG_${name}=${getAttr name attrs} ";
        in foldl' op "" (attrNames attrs);

    in stdenv.mkDerivation {
      name = "genode-${repo}-${version}";
      outputs = [ "out" "dev" ];
      inherit src repo specs version;

      setupHook = ./setup-hooks.sh;

      nativeBuildInputs = repoInputs;
      # This is wrong, why does pkg-config not collect buildInputs?

      propagatedNativeBuildInputs = repoInputs;

      depsBuildBuild = with buildPackages; [ llvm pkgconfig tup ];

      tupConfig = toTupConfig {
        LIBCXX = llvmPackages.libcxx;
        LIBCXXABI = llvmPackages.libcxxabi;
        LIBUNWIND = llvmPackages.libunwind;
        LIBUNWIND_BAREMETAL =
          llvmPackages.libunwind.override { isBaremetal = true; };
        LINUX_HEADERS = buildPackages.glibc.dev;
        OLEVEL = "-O2";
        TUP_ARCH = tupArch;
        VERSION = version;
      };

      configurePhase = ''
        # Configure Tup
        echo $tupConfig | tr ' CONFIG_' '\nCONFIG_' > tup.config
        echo CONFIG_NIX_OUTPUTS_OUT=$out >> tup.config
        echo CONFIG_NIX_OUTPUTS_DEV=$dev >> tup.config

        # Disable other repos
        for R in repos/*; do
          [ "$R" != "repos/$repo" ] && find $R -name Tupfile -delete
        done
        find repos/gems -name Tupfile -delete

        # Scan repository and generate script
        tup init
        tup generate buildPhase.sh

        # Redirect artifacts to Nix store
        mkdir -p $out/lib $dev/include
        ln -s $out out
        ln -s $dev dev
      '';

      buildPhase = ''
        test -d repos/$repo/src/ld && cp -rv repos/$repo/src/ld $dev/
        pushd .
        set -v
        source buildPhase.sh
        set +v
        popd
      '';

      installPhase = ''
        # Populate the "dev" headers
        if [ -d "repos/$repo/include" ]; then
          for DIR in repos/$repo/include; do
            for SPEC in $specs; do
              if [ -d $DIR/spec/$SPEC ]; then
                cp -r $DIR/spec/$SPEC/* $dev/include
                rm -r $DIR/spec/$SPEC
              fi
            done
            rm -rf $DIR/spec
            cp -r $DIR $dev/
          done
        fi

        touch $dev/.genode
        for pc in $dev/lib/pkgconfig/*.pc; do
          sed -e "s|^Libs: |Libs: -L$dev/lib |" -i $pc
        done
      '';

      meta = with stdenv.lib; {
        description =
          "The Genode operation system framework (${repo} repository).";
        homepage = "https://genode.org/";
        license = licenses.agpl3;
        maintainers = [ maintainers.ehmry ];
      };

      shellHook = ''
        export PROMPT_DIRTRIM=2
        export PS1="\[\033[1;30m\]Genode-dev [\[\033[1;37m\]\w\[\033[1;30m\]] $\[\033[0m\] "
        export PS2="\[\033[1;30m\]>\[\033[0m\] "
        if [ -e "configs/.gitignore" ]; then
          local CFG=configs/${targetPlatform.config}.config
          echo $tupConfig | tr ' CONFIG_' '\nCONFIG_' > $CFG
          tup variant $CFG
        fi
      '';
    };

in rec {
  base = buildRepo {
    repo = "base";
    repoInputs = [ ];
  };

  base-linux = buildRepo {
    repo = "base-linux";
    repoInputs = [ base ];
  };

  base-nova = buildRepo {
    repo = "base-nova";
    repoInputs = [ base ];
  };

  os = buildRepo {
    repo = "os";
    repoInputs = [ base ];
  };

  gems = buildRepo {
    repo = "gems";
    repoInputs = [ os ];
  };
}
