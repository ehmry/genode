{ pkgs ? import <nixpkgs> {} }: with pkgs;

let
  genodeVersion = "19.05";
  glibcVersion = (builtins.parseDrvName stdenv.glibc.name).version;

in
stdenv.mkDerivation rec {
  name = "genode-toolchain-${genodeVersion}";
  version = genodeVersion;

  src =
    if stdenv.isx86_64 then
    fetchurl {
      url = "https://downloads.sourceforge.net/project/genode/genode-toolchain/${genodeVersion}/genode-toolchain-${genodeVersion}-x86_64.tar.xz";
      sha256 = "036czy21zk7fvz1y1p67q3d5hgg8rb8grwabgrvzgdsqcv2ls6l9";
    }
    else abort "no toolchain for ${stdenv.system}";

  buildInputs = [ patchelf ];

  dontPatchELF = true;

  # installPhase is disabled for now
  phases = "unpackPhase fixupPhase";

  unpackPhase = ''
    mkdir -p $out

    echo "unpacking $src..."
    tar xf $src --strip-components=5 -C $out
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
