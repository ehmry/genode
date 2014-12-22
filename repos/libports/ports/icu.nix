{ preparePort, fetchurl }:

preparePort rec {
  version = "51.2";
  name = "icu-${version}";
  outputs = [ "source" "include" ];

  src = fetchurl {
    url = http://download.icu-project.org/files/icu4c/51.2/icu4c-51_2-src.tgz;
    sha256 = "1k04sxxxqzx6zx7gkri27ilfk14cna9zp5lb541yqfqvbyh2gc6y";
  };

  installPhase =
    ''
      cp -r source $source

      mkdir -p $include/icu/common $include/icu/i18n
      cp -r source/common/*.h source/common/unicode \
          $include/icu/common/
      cp -r source/i18n/*h source/i18n/unicode \
          $include/icu/i18n/
    '';
}
