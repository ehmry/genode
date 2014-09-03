{ cross, gccSources
, stdenv, fetchurl, zlib
, binutils, gmp, mpfr, mpc, libcCross
, langC ? true, langCC ? true, langGo ? false
}:

# The go frontend is written in c++
assert langGo -> langCC;

with stdenv.lib;

let
  version = gccSources.version;
    
  ldDefsymDummyLibc = map (s: "-Wl,--defsym=${s}=0") [ "strlen" "free" "memcpy" "malloc" "memset" "abort" "dl_iterate_phdr" ];

  # this comprises the rules of how the GCC frontend invokes the linker.
  linkSpecH = builtins.toFile "link_spec.h" (
    if cross.platform == "arm" then
      "#define LINK_SPEC \"%(shared:-shared) %{!static:--eh-frame-hdr}\"" else
    if cross.platform  == "x86" then
      "#define LINK_SPEC \"%{!m32:-m elf_x86_64} %{m32:-m elf_i386} %{shared:-shared} %{!static:--eh-frame-hdr}\"" else
    throw "Unknown toolchain target platform ${cross.platform}"
  );

in 
stdenv.mkDerivation ( cross // {
  name = "genode-gcc-${version}";

  buildInputs = [ binutils gmp mpfr mpc zlib ];

  unpackPhase = ''
    mkdir build
    cd build
  '';

  preConfigure = ''
    host_configargs="host_xm_include_list=${linkSpecH} tmake_file='t-slibgcc'";

    #
    # Passed to target components such as libgcc, libstdc++
    #
    # The 't-slibgcc' tmake file is needed to have libgcc_eh.a built.
    # The 't-eh-dw2-dip' tmake file is needed to let the tool chain use 'unwind-dw2-fde-dip.c',
    # needed for the exception handling on Genode in the presence of shared libraries.
    #
    target_configargs="tmake_file='t-crtstuff-pic t-libgcc-pic t-eh-dw2-dip t-slibgcc t-slibgcc-gld t-slibgcc-elf-ver' \
                       extra_parts='crtbegin.o crtbeginS.o crtbeginT.o crtend.o crtendS.o' \
                       PIC_CXXFLAGS='-prefer-pic'"

    configureFlagsArray=(
      CPPFLAGS_FOR_TARGET=-I${libcCross}/include
      CFLAGS_FOR_TARGET="-I${libcCross}/include -nostdlib ${toString ldDefsymDummyLibc} -fPIC"
    );

    configureScript=${gccSources.out}/configure
  '';

  configureFlags = cross.configureFlags ++ [
    "--enable-languages=${concatStrings (intersperse ","
        (  optional langC  "c"
        ++ optional langCC "c++"
        ++ optional langGo "go"
        ))}"
    "--disable-libgo"
    "--enable-targets=all"
    "--with-gnu-as" "--with-gnu-ld" "--disable-tls" "--disable-threads"
    "--disable-libstdcxx-pch"
    "--enable-shared"
    "--with-gmp=${gmp}"
    "--with-mpfr=${mpfr}"
    "--with-mpc=${mpc}"
  ];

  installTargets = "install-strip";

  meta.broken = true;

})