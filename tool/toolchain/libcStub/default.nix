{ stdenv }:

let stub = ../../libgcc_libc_stub.h; in
stdenv.mkDerivation {
  name = "libgcc-libc-stub";

  builder = builtins.toFile "builder.sh" ''
    source $stdenv/setup
    mkdir -p $out/include/sys

    for header in stdint.h memory.h string.h stdlib.h \
      unistd.h errno.h wchar.h ctype.h \
      strings.h wctype.h math.h stdio.h \
      dlfcn.h inttypes.h malloc.h signal.h \
      fcntl.h assert.h locale.h setjmp.h \
      time.h link.h gnu-versions.h elf.h;    do
      ln -s ${stub} $out/include/$header
    done

    for i in types.h stat.h sem.h ;    do
      ln -s ${stub} $out/include/sys/$i
    done
  '';

}