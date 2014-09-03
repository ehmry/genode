/*
 * \brief  Function to compile a source file to an object file
 * \author Emery Hemingway
 * \date   2014-08-6
 */

{ spec, compiler, common, shell }:

let
  result = derivation
    { system = builtins.currentSystem;
      name = "${spec.system}-compile";

      inherit common;
      script = ./compile.sh;

      builder = shell;
      args = [ "-e" ./builder.sh ];
    };

  ccFlags = spec.ccMarch ++ 
    [ # Always compile with '-ffunction-sections' to enable the use of the
      # linker option '-gc-sections'
      "-ffunction-sections"

      # Prevent the compiler from optimizations related to strict aliasing
      "-fno-strict-aliasing"

      # Do not compile with standard includes.
      "-nostdinc"

      "-g" "-O2" "-Wall"

      "-fPIC"
    ];

    cxxFlags = ccFlags ++ [ "-std=gnu++11" ];

in

{ source, includeDirs, flags }:
derivation {
  name = "${spec.system}-object";
  system = builtins.currentSystem;

  inherit source includeDirs flags;

  inherit (compiler) cc cxx;
  inherit ccFlags cxxFlags;

  builder = shell;
  args = [ "-e" "${result}/compile.sh" ];

  verbose = builtins.getEnv "VERBOSE";

}
