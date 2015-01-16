{ spec, linkSharedLibrary, compileCCRepo, nixSrc, shellDerivation, genodeEnv
, libnixutil, libbz2, sqlite }:

let
  sqlInclude = shellDerivation {
    name = "schema";
    input = "${nixSrc}/src/libstore/schema.sql";
    inherit genodeEnv;
    script = builtins.toFile "schema.sh"
      ''
        source $genodeEnv/setup
        mkdir $out
        sed -e 's/"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < $input > $out/schema.sql.hh
      '';
  };

  sourceRoot = "${nixSrc}/src/libstore";
in
linkSharedLibrary rec {
  name = "libnixstore";
  libs = [ libnixutil libbz2 sqlite ];
  externalObjects = compileCCRepo {
    inherit name libs sourceRoot;
    sources = "*.cc";
    systemIncludes = [ ./include "${nixSrc}/src" sqlInclude ];
    extraFlags =
      [ ''-DSYSTEM=\"${spec.system}-genode\"''
        ''-DNIX_STORE_DIR=\"/nix/store\"''
        ''-DNIX_DATA_DIR=\"datadir\"''
        ''-DNIX_LOG_DIR=\"logdir\"''
        ''-DNIX_STATE_DIR=\"statedir\"''
        ''-DNIX_CONF_DIR=\"confdir\"''
        ''-DNIX_LIBEXEC_DIR=\"libexecdir\"''
        ''-DNIX_BIN_DIR=\"bindir\"''
        ''-DOPENSSL_PATH=\"openss\"''
      ];
  };
  propagate.systemIncludes = [ sourceRoot ];
}
