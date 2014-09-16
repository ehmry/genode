/*
* \brief  Toolchain expressions for non-Genode hosts
* \author Emery Hemingway
* \date   2014-08-11
*
* This was an attempt to recreate the toolchain with nix,
* and then a frustrated fallback to patching the precompiled binaries
* to make them work.
*/

{ spec, nixpkgs }:

let
  cross = rec {
    configureFlags = [
      "--target=${spec.target}"
      #"--program-prefix=genode-${spec.platform}-"
      #"--program-transform-name='s/${spec.target}/${spec.platform}/'"
    ];
    preBuild =
      if spec.target == "x86_64-elf" then ''
      makeFlagsArray=(MULTILIB_OPTIONS="m64/m32" MULTILIB_DIRNAMES="64 32")
    '' else throw "unsupported target ${spec.target}";
  };

  commonLibConfigureFlags = [
    "--disable-shared" "--enable-static"
  ];

in rec {
  /*

  binutils = import ./binutils {
    inherit cross;
    inherit (nixpkgs) stdenv fetchurl;
  };

  gmp = import ./gmp {
    inherit (nixpkgs) stdenv fetchurl m4;
    inherit commonLibConfigureFlags;
  };

  mpfr = import ./mpfr {
    inherit (nixpkgs) stdenv fetchurl;
    inherit commonLibConfigureFlags gmp;
  };

  mpc = import ./mpc {
    inherit (nixpkgs) stdenv fetchurl;
    inherit commonLibConfigureFlags gmp mpfr;
  };

  libc = import ./libcStub {
    inherit (nixpkgs) stdenv;
  };

  # unpacking gcc sources into the nix store saves
  # time when compiling gcc for multiple targets.
  gccSources = import ./gcc-sources {
    inherit (nixpkgs) stdenv fetchurl;
  };

  gcc = import ./gcc {
   inherit cross binutils gmp mpfr mpc gccSources;
   inherit (nixpkgs) stdenv fetchurl zlib; 
   libcCross = libc;
  };
  */

  precompiled = nixpkgs.callPackage ./precompiled {
    glibc = nixpkgs.glibc_multi;
  };

}