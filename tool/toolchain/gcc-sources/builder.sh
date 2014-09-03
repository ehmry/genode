source $stdenv/setup

mkdir -p $out
tar xpf $src --strip-components=1 -C $out

cd $out
rm -r zlib
patchPhase
