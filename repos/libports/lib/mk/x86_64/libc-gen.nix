/*
 * \author Emery Hemingway
 * \date   2014-09-20
 */

{ tool, libc }: with tool;

let
  generic = import ../libc-gen.inc.nix { inherit tool libc; };

  filterS =
    [ "rfork_thread.S" "setjmp.S" "_setjmp.S"

      # Filter functions conflicting with libm
      "fabs.S" "modf.S"
    ];
  filterC =
    [ "_set_tc.c"
      
      # Filter functions conflicting with libm
      "frexp.c"
    ];
in
buildLibrary {
  name = "libc-gen";
  sources =
    generic.sources
    ++
    (filterOut 
      filterS 
      (wildcard "${libc}/src/lib/libc/lib/libc/amd64/gen/*.S"))
    ++ 
    [ "${libc}/src/lib/libc/lib/libc/amd64/gen/flt_rounds.c" ];
  inherit (generic) includeDirs;
}