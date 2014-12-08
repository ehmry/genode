{ genodeEnv, preparePort, fetchurl }:

let
  inherit (genodeEnv.toolchain) version;
in
preparePort {
  name = "stdcxx-${version}";

  src = fetchurl {
    url = "mirror://gnu/gcc/gcc-${version}/gcc-${version}.tar.bz2";
    sha256 = "10k2k71kxgay283ylbbhhs51cl55zn2q38vj5pk4k950qdnirrlj";
  };

  tarFlags = "gcc-${version}/libstdc++-v3 --strip-components=2";

  sourceRoot = ".";

  installPhase =
    ''
      mkdir $source
      cp -r src config libsupc++ $source

      cp -r include $include
    '';

  outputs = [ "source" "include" ];

}
