{ spec, linkSharedLibrary, compileCC, compileCCRepo, fromDir
, shellDerivation, genodeEnv, filterHeaders
, nixSrc, nixutil, libbz2, sqlite }:

let
  sourceRoot = "${nixSrc}/src/libstore";
  extraFlags =
    [ ''-DSYSTEM=\"${spec.system}-genode\"''
      ''-DNIX_BIN_DIR=\"/nix/bin\"''
      ''-DNIX_LIBEXEC_DIR=\"/nix/libexec\"''
      ''-DOPENSSL_PATH=\"openssl\"''
    ];

  sqlInclude = shellDerivation {
    name = "schema";
    input = "${sourceRoot}/schema.sql";
    inherit genodeEnv;
    script = builtins.toFile "schema.sh"
      ''
        source $genodeEnv/setup
        mkdir $out
        sed -e 's/"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < $input > $out/schema.sql.hh
      '';
  };
in
linkSharedLibrary rec {
  name = "nixstore";
  libs = [ nixutil libbz2 ];
  objects = map
    (src: compileCC {
      inherit src libs extraFlags;
      externalIncludes =
        [ sourceRoot sqlInclude ];
    })
    [ ./globals.cc
      ./store-api.cc
      ./nichts_store.cc
    ];

  externalObjects = compileCCRepo {
    inherit libs extraFlags;
    sources = fromDir
      sourceRoot
      [ "derivations.cc" "misc.cc" ];
  };

  propagate.externalIncludes = [ "${nixSrc}/src" sourceRoot ];
}
