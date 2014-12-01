{ stdenv, fetchurl, patchelf, glibc, zlib, gcc }:

let genodeVersion = "14.11"; in
stdenv.mkDerivation rec {
  name = "genode-toolchain-${genodeVersion}";
  version = "4.7.4";

  src = if stdenv.isi686 then
    fetchurl {
      url = "mirror://sourceforge/genode/genode-toolchain/${genodeVersion}/${name}-x86_32.tar.bz2";
      sha256 = "04rvsbmm7jlk7gkja0sag7m0pb996pzb02fxmx9dkksxn7cg2v0q";
    } else
    if stdenv.isx86_64 then
    fetchurl {
      url = "mirror://sourceforge/genode/genode-toolchain/${genodeVersion}/${name}-x86_64.tar.bz2";
      sha256 = "0xjwrvnlmcq3p8v4mdj20gwba1qd1bk4khs1q1cbrzl24vqa1l85";
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
    interp=${glibc}/lib/ld-2.19.so
    if [ ! -f "$interp" ] ; then
       echo new interpreter $interp does not exist,
       echo cannot patch binaries
       exit 1
    fi

    for f in $(find $out); do
        if [ -f "$f" ] && patchelf "$f" 2> /dev/null; then
            patchelf --set-interpreter $interp \
                     --set-rpath $out/lib:${glibc}/lib:${zlib}/lib \
                "$f" || true
        fi
    done
  '';

  passthru = { inherit glibc; };
}
