{ cross, stdenv, fetchurl }:
let
  version = "2.22";
in
stdenv.mkDerivation (cross // {
  name = "genode-binutils-${version}";

  src = fetchurl {
    url = "mirror://gnu/binutils/binutils-${version}.tar.bz2";
    sha256 = "1a9w66v5dwvbnawshjwqcgz7km6kw6ihkzp6sswv9ycc3knzhykc";
  };

  patches = [
    # Turn on --enable-new-dtags by default to make the linker set
    # RUNPATH instead of RPATH on binaries.  This is important because
    # RUNPATH can be overriden using LD_LIBRARY_PATH at runtime.
    #./new-dtags.patch

    # Since binutils 2.22, DT_NEEDED flags aren't copied for dynamic outputs.
    # That requires upstream changes for things to work. So we can patch it to
    # get the old behaviour by now.
    #./dtneeded.patch

    # Make binutils output deterministic by default.
    ./deterministic.patch
  ];

  #buildInputs = [ nixpkgs.zlib ];

  preConfigure = ''
    # Clear the default library search path.
    echo 'NATIVE_LIB_DIRS=' >> ld/configure.tgt
  '';

  configureFlags = cross.configureFlags ++ [
    "--disable-werror"

    # Prevent GNU assembler from treating '/' as the start of a comment. In
    # 'gas/config/tc-i386.c', the policy of handling '/' is defined. For Linux, '/'
    # is treated as division, which we expect. To apply this needed policy for our
    # plain 'elf' version gas, we supply the definition of 'TE_LINUX' by hand.
    # Fortunately, this define is not used outside of gas.
    "CFLAGS=-DTE_LINUX"

    "--enable-shared" "--enable-deterministic-archives"
  ];

  installPhase = ''
    for i in binutils gas ld intl opcodes; do
      make -C $i install-strip
    done

    make -C libiberty install
  '';
})