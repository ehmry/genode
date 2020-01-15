let
  pinnedNixpkgs = import (builtins.fetchGit {
    url = "https://gitea.c3d2.de/ehmry/nixpkgs.git";
    ref = "genode";
  });
in { localSystem ? "x86_64-linux", crossSystem ? "x86_64-genode"
, nixpkgs ? pinnedNixpkgs, self ? { }, dhall-haskell ? null }:

let
  nixpkgs' = if builtins.isAttrs nixpkgs then
    nixpkgs
  else
    nixpkgs { inherit localSystem crossSystem; };

  inherit (nixpkgs') buildPackages llvmPackages;

  sourceForgeToolchain = nixpkgs'.buildPackages.callPackage ./toolchain.nix { };

  stdenvLlvm = let inherit (nixpkgs') stdenv;
  in assert stdenv.cc.isClang; stdenv;

  stdenvGcc = let
    env =
      nixpkgs'.stdenvAdapters.overrideCC nixpkgs'.stdenv sourceForgeToolchain;
  in assert env.cc.isGNU; env;

  inherit (stdenvLlvm) lib targetPlatform;
  specs = with targetPlatform;
    [ ]

    ++ lib.optional is32bit "32bit"

    ++ lib.optional is64bit "64bit"

    ++ lib.optional isAarch32 "arm"

    ++ lib.optional isAarch64 "arm_64"

    ++ lib.optional isRiscV "riscv"

    ++ lib.optional isx86 "x86"

    ++ lib.optional isx86_32 "x86_32"

    ++ lib.optional isx86_64 "x86_64";

  toTupConfig = env: attrs:
    let
      tupArch = with env.targetPlatform;

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

      attrs' = with env; { TUP_ARCH = tupArch; } // attrs;

    in with builtins;
    env.mkDerivation {
      name = "tup.config";
      nativeBuildInputs = with nixpkgs'.buildPackages; [
        binutils
        pkgconfig
        which
      ];
      text = let
        op = config: name: ''
          ${config}CONFIG_${name}=${getAttr name attrs}
        '';
      in foldl' op "" (attrNames attrs);
      passAsFile = [ "text" ];
      preferLocalBuild = true;
      buildCommand = let
        subst = let
          vars = [ "AR" "NM" ];
          f = other: var:
            other + ''
              echo CONFIG_${var}=`which ''$${var}` >> $out
            '';
        in foldl' f "" vars;
        utils = let
          vars = [ "pkg-config" "objcopy" ];
          f = other: var:
            other + ''
              echo CONFIG_${var}=`which ${var}` >> $out
            '';
        in foldl' f "" vars;
      in ''
        cp $textPath $out
        ${subst}
        ${utils}
      '';
    };

  tupConfigGcc = let
    f = env:
      let prefix = bin: env.cc.targetPrefix + bin;
      in {
        CC = prefix "gcc";
        CXX = prefix "g++";
        LD = prefix "ld";
        OBJCOPY = prefix "objcopy";
        RANLIB = prefix "ranlib";
        READELF = prefix "readelf";
        STRIP = prefix "strip";
        PKGCONFIG = "${nixpkgs'.buildPackages.pkgconfig}/bin/pkg-config";

        IS_GCC = "";
        LINUX_HEADERS = buildPackages.glibc.dev;
      };
  in toTupConfig stdenvGcc (f stdenvGcc);

  tupConfigLlvm = let
    f = env:
      let prefix = bin: "${env.cc}/bin/${env.cc.targetPrefix}${bin}";
      in {
        CC = prefix "cc";
        CXX = prefix "c++";
        LD = prefix "ld";
        OBJCOPY = prefix "objcopy";
        OBJDUMP = prefix "objdump";
        RANLIB = prefix "ranlib";
        READELF = prefix "readelf";
        STRIP = prefix "strip";
        PKGCONFIG = "${nixpkgs'.buildPackages.pkgconfig}/bin/pkg-config";

        IS_LLVM = "";
        LIBCXXABI = llvmPackages.libcxxabi;
        LIBCXX = llvmPackages.libcxx;
        LIBUNWIND_BAREMETAL =
          llvmPackages.libunwind.override { isBaremetal = true; };
        LIBUNWIND = llvmPackages.libunwind;
        LINUX_HEADERS = buildPackages.glibc.dev;
      };
  in toTupConfig stdenvLlvm (f stdenvLlvm);

  buildRepo = { env, repo, repoInputs, filter }:
    let

    in env.mkDerivation {
      name = "genode-" + repo;
      inherit repo specs;

      src = builtins.filterSource filter ./.;

      nativeBuildInputs = repoInputs;
      # This is wrong, why does pkg-config not collect buildInputs?

      propagatedNativeBuildInputs = repoInputs;

      depsBuildBuild = with buildPackages; [ llvm pkgconfig tup ];

      tupConfig = if env.cc.isGNU then
        tupConfigGcc
      else if env.cc.isClang then
        tupConfigLlvm
      else
        throw "no Tup config for this stdenv";

      configurePhase = ''
        # Configure Tup
        set -v
        install -m666 $tupConfig tup.config
        echo CONFIG_NIX_OUTPUTS_OUT=$out >> tup.config
        echo CONFIG_NIX_OUTPUTS_DEV=$out >> tup.config

        # Disable other repos
        for R in repos/*; do
          [ "$R" != "repos/$repo" ] && find $R -name Tupfile -delete
        done

        # Scan repository and generate script
        tup init
        tup generate buildPhase.sh

        # Redirect artifacts to Nix store
        mkdir -p $out/lib $out/include
        ln -s $out out
        ln -s $out dev
      '';

      buildPhase = ''
        test -d repos/$repo/src/ld && cp -rv repos/$repo/src/ld $out/
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
                cp -r $DIR/spec/$SPEC/* $out/include
                rm -r $DIR/spec/$SPEC
              fi
            done
            rm -rf $DIR/spec
            cp -r $DIR $out/
          done
        fi

        touch $out/.genode
        for pc in $out/lib/pkgconfig/*.pc; do
          sed -e "s|^Libs: |Libs: -L$out/lib |" -i $pc
        done
      '';

      meta = with env.lib; {
        description =
          "The Genode operation system framework (${repo} repository).";
        homepage = "https://genode.org/";
        license = licenses.agpl3;
        maintainers = [ maintainers.ehmry ];
      };

    };

  buildRepo' = { ... }@args: buildRepo ({ env = stdenvGcc; } // args);

  #builtins.throw "create the tup config file in the stdenv environment, replacing CC/CXX with environmental variables"

in rec {
  packages = let

    hasPrefix = pre:
      with builtins;
      let
        pre' = "${toString ./.}/${pre}";
        preLen = stringLength pre';
      in path:
      let pathLen = stringLength path;
      in substring 0 preLen path == substring 0 pathLen pre';

    hasSuffix = suf:
      with builtins;
      let sufLen = stringLength suf;
      in path:
      let pathLen = stringLength path;
      in substring (pathLen - sufLen) pathLen path == suf;

    filterBaseRepo = name:
      with builtins;
      let
        match = [
          (hasPrefix "Tup")
          (hasPrefix "repos/Tup")
          (hasPrefix "repos/base/")
          (hasPrefix "repos/base-${name}")
        ];
        skip = [
          (hasSuffix "/run")
          (hasSuffix "/recipes")
          (hasSuffix ".gitignore")
          (path: hasPrefix "repos/base/" path && hasSuffix "Tupfile" path)
        ];
      in path: type: let f = f': f' path; in any f match && !any f skip;

    filterRepo = repo:
      with builtins;
      let
        reposDir = toString ./repos;
        skip = [
          (hasSuffix "/run")
          (hasSuffix "/recipes")
          (hasSuffix ".gitignore")
        ];
        match = [
          (hasPrefix "Tup")
          (hasPrefix "repos/Tup")
          (hasPrefix "repos/${repo}")
          (hasPrefix "repos/base/src/ld")
        ];
      in path: type: let f = f': f' path; in any f match && !any f skip;
  in rec {

    NOVA = nixpkgs.callPackage ./NOVA { };

    base = buildRepo' {
      repo = "base";
      repoInputs = [ ];
      filter = with builtins;
        let
          match = [
            (hasPrefix "Tup")
            (hasPrefix "repos/Tup")
            (hasPrefix "repos/base/")
          ];
          skip = [
            (hasSuffix "/run")
            (hasSuffix "/recipes")
            (hasSuffix ".gitignore")
          ];
        in path: type: let f = f': f' path; in any f match && !any f skip;
    };

    base-linux = buildRepo' {
      repo = "base-linux";
      repoInputs = [ base ];
      filter = filterBaseRepo "linux";
    };

    base-nova = buildRepo' {
      repo = "base-nova";
      repoInputs = [ base ];
      filter = filterBaseRepo "nova";
    };

    os = buildRepo' {
      repo = "os";
      repoInputs = [ base ];
      filter = filterRepo "os";
    };

    gems = buildRepo' {
      repo = "gems";
      repoInputs = [ base os ];
      filter = filterRepo "gems";
    };

    inherit stdenvGcc stdenvLlvm tupConfigGcc tupConfigLlvm;

    nova-image = import ./apps/nova-image {
      stdenv = stdenvLlvm;
      inherit nixpkgs NOVA base-nova;
      dhallApps = dhall-haskell.apps.${localSystem};
    };

    nova-iso = import ./apps/nova-iso {
      stdenv = stdenvLlvm;
      inherit nixpkgs NOVA base-nova;
      dhallApps = dhall-haskell.apps.${localSystem};
    };
  };

  apps = {
    core-linux = {
      type = "app";
      program = "${packages.base-linux}/bin/core-linux";
    };
    nova-image = {
      type = "app";
      program = "${packages.nova-image}/bin/nova-image";
    };
    nova-iso = {
      type = "app";
      program = "${packages.nova-iso}/bin/nova-iso";
    };
  };

  defaultApp = apps.core-linux;
  defaultPackage = packages.base-linux;
  devShell = packages.base;
  checks = packages;
}
