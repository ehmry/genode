{ pkgs ? import <nixpkgs> {} }: with pkgs;

let
  genodeVersion = "16.05";
  glibcVersion = (builtins.parseDrvName stdenv.glibc.name).version;

in
stdenv.mkDerivation rec {
  name = "genode-toolchain-${genodeVersion}";
  version = "4.9.2";

  src = if stdenv.isi686 then
    fetchurl {
      url = "mirror://sourceforge/genode/genode-toolchain/${genodeVersion}/${name}-x86_32.tar.bz2";
      sha256 = "12ykyw7x8vyz7f8kb8qh7zyc4x3w2m10pqnqy1rwh4mjpx60b0cp";
    } else
    if stdenv.isx86_64 then
    fetchurl {
      url = "mirror://sourceforge/genode/genode-toolchain/${genodeVersion}/${name}-x86_64.tar.bz2";
      sha256 = "07k41p2ssr6vq793g766y5ng14ljx9x5d5qy2zvjkq7csqr9hr1j";
    }
    else abort "no toolchain for ${stdenv.system}";

  buildInputs = [ patchelf ];

  dontPatchELF = true;

  # installPhase is disabled for now
  phases = "unpackPhase fixupPhase";

  unpackPhase = ''
    mkdir -p $out

    echo "unpacking $src..."
    tar xf $src --strip-components=3 -C $out
  '';

  installPhase = ''
    cd $out/bin
    for platform in arm x86 ; do
        dest="$"$platform"/bin"
        eval dest=$"$dest"

        mkdir -p $dest

        for b in genode-$platform-* ; do
            eval ln -s $b $dest/$\{b#genode-$platform-\}
        done

    done
    cd -
  '';

  fixupPhase = ''
    interp=${stdenv.glibc.out}/lib/ld-${glibcVersion}.so
    if [ ! -f "$interp" ] ; then
       echo new interpreter $interp does not exist,
       echo cannot patch binaries
       exit 1
    fi

    for f in $(find $out); do
        if [ -f "$f" ] && patchelf "$f" 2> /dev/null; then
            patchelf --set-interpreter $interp \
                     --set-rpath $out/lib:${stdenv.glibc.out}/lib:${zlib.out}/lib \
                "$f" || true
        fi
    done
  '';

  passthru = { libc = stdenv.glibc; };
}
