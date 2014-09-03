/*
 * \brief  
 * \author Emery Hemingway
 * \date   2014-08-16
 *
 * It took me a while to figure out why this didn't work,
 * then I realized I need to use the host GCC.
 * It still doesn't work.
 *
 */

 { build, base }:

build.test rec {
  name = "test-lx_hybrid_ctors";
  libs = [ base.lib.lx_hybrid ];
  sourceDir = ../lx_hybrid_ctors;
  sources = [ "${sourceDir}/main.cc" ];
  includeDirs = [ sourceDir ] ++ base.includeDirs;

  phases = [ "buildTestLib" "mergeStaticPhase" ];
  testlibSrc = ./testlib.cc;
  buildTestLib = ''
    includeOpt=""
    for d in $includeDirs; do
        includeOpt="$includeOpt -I$d"
    done

    MSG_BUILD libtestlib.so
    source ${build.nixpkgs.stdenv}/setup
    VERBOSE g++ $ccMarch -fPIC -I$sourceDir -I$libc/include \
        -c $sourceDir/testlib.cc -o testlib.o
    VERBOSE g++ $ccMarch -shared \
        -o libtestlib.so testlib.o
  '';

}