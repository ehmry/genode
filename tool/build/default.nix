/*
 * \brief  Functions for building Genode components
 * \author Emery Hemingway
 * \date   2014-08-11
 *
 * Much of the what is imported here needs to be
 * de-duplicated.
 *
 */

{ spec, nixpkgs, toolchain }:

let
  devPrefix = "genode-${spec.platform}-";
  shell = nixpkgs.bash + "/bin/sh";

  build = rec {
    inherit spec nixpkgs toolchain;

    version = builtins.readFile ../../VERSION;

    is32Bit  = spec.bits == 32;
    is64Bit  = spec.bits == 64;
    isArm = spec.platform == "arm";
    isx86 = spec.platform == "x86";
    isx86_32 = isx86 && is32Bit;
    isx86_64 = isx86 && is64Bit;

    isLinux = spec.kernel == "linux";
    isNova  = spec.kernel == "nova";

    cc      = "${devPrefix}gcc";
    cxx     = "${devPrefix}g++";
    ld      = "${devPrefix}ld";
    as      = "${devPrefix}as";
    ar      = "${devPrefix}ar";
    nm      = "${devPrefix}nm";
    objcopy = "${devPrefix}objcopy";
    ranlib  = "${devPrefix}ranlib";
    strip   = "${devPrefix}strip";

    # Options for automatically generating dependency files
    #
    # We specify the target for the generated dependency file explicitly via
    # the -MT option. Unfortunately, this option is handled differently by
    # different gcc versions. Older versions used to always append the object
    # file to the target. However, gcc-4.3.2 takes the -MT argument literally.
    # So we have to specify both the .o file and the .d file. On older gcc
    # versions, this results in the .o file to appear twice in the target
    # but that is no problem.
    ccOptDep = [ "-MMD" ];

    ccOLevel = "-O2";
    ccWarn   = "-Wall";

    ccOpt = [
      # Always compile with '-ffunction-sections' to enable 
      # the use of the linker option '-gc-sections'
      "-ffunction-sections"

      # Prevent the compiler from optimizations related to strict aliasing
      "-fno-strict-aliasing"

      # Do not compile with standard includes per default.
      "-nostdinc"

      "-g"
    ]
    ++ spec.ccOpt
    ++ spec.ccMarch;

    ccCxxOpt = [ "-std=gnu++11" ];
    ccCOpt = ccOpt;

    ldOpt = spec.ldMarch ++ [ "-gc-sections" ];
    cxxLinkOpt = [ ];

    # Linker script for dynamically linked programs
    ldScriptDyn = ../../repos/os/src/platform/genode_dyn.ld;

    # Linker script for shared libraries
    ldScriptSo  = ../../repos/os/src/platform/genode_rel.ld;

    asOpt = spec.asMarch;

    ##########################################################

  };

    common = import ./common { inherit spec nixpkgs toolchain; };

    compileObject = import ./compile-object { inherit build common shell; };
    transformBinary = import ./transform-binary { inherit build common shell; };

in
(build // rec {

  library = import ./library { inherit build common compileObject transformBinary shell; };
  component = import ./component { inherit build common compileObject transformBinary shell; };

  # These might be overridden later
  driver = component; server = component; test = component;

})