{ genodeEnv, compileCC, base, libpng_static }:

genodeEnv.mkComponent {
  name = "test-libpng_static";
  libs = [ base libpng_static ];

  objects = compileCC {
    src = ./main.cc;
    extraFlags =
      [ "-funroll-loops" "-DPNG_USER_CONFIG" "-Wno-address" ];
    systemIncludes =
      [ ../../../include/libpng_static
        ../../../include/libz_static
        ../../../include/mini_c
      ];
  };
}
