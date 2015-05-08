{ preparePort, fetchurl }:

preparePort rec {
  name = "readline-${version}";
  version = "6.0";
  outputs = [ "source" "include" ];
  src = fetchurl {
    url = "mirror://gnu/readline/${name}.tar.gz";
    sha256 = "1pn13j6f9376kwki69050x3zh62yb1w31l37rws5nwr5q02xk68i";
  };

  installPhase =
    ''
      mkdir -p $include/readline $source
      cp \
          rlstdc.h rltypedefs.h keymaps.h tilde.h \
          readline.h history.h \
          chardefs.h \
          $include/readline
      cp *.c *.h $source
      ln -s $source $source/readline
    '';
}