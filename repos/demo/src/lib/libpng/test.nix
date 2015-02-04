{ linkComponent, compileCC, base, libpng_static }:

linkComponent {
  name = "test-libpng_static";
  libs = [ base libpng_static ];

  objects = compileCC {
    src = ./main.cc;
    extraFlags =
      [ "-funroll-loops" "-DPNG_USER_CONFIG" "-Wno-address" ];
    includes =
      [ ../../../include/libpng_static
        ../../../include/libz_static
        ../../../include/mini_c
      ];
  };
}
