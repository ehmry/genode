/*
 * \brief  Generic portion of lib-gen
 * \author Emery Hemingway
 * \date   2014-09-30
 *
 */

{ tool, libc }: with tool;

let
  # this file produces a warning about a missing header file,
  # lets drop it
  filter =
    [ "getosreldate.c" "sem.c" "valloc.c" "getpwent.c" ];

  libcGenDir = "${libc}/src/lib/libc/lib/libc/gen/";

  repoSrcDir = ../../src/lib/libc;
in
{
  sources = filterOut filter (wildcard "${libcGenDir}*.c");

  includeDirs =
    [ # 'sysconf.c' includes the local 'stdtime/tzfile.h'
      "${libc}/src/lib/libc/lib/libc/stdtime"

      # '_pthread_stubs.c' includes the local 'libc_pdbg.h'
      ../../src/lib/libc
    ];
}