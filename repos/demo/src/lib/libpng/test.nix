{ build, base, os, demo }:
build.test {
  name = "test-libpng_static";
  ccOpt = [ "-funroll-loops" "-DPNG_USER_CONFIG" "-Wno-address" ] ++ build.ccOpt;
  libs =
    [ base.lib.base
      demo.lib.libpng_static
    ];
  sources = [ ./main.cc ];
  includeDirs =
    [ (demo.includeDir + "/libpng_static")
      (demo.includeDir + "/mini_c")
      (demo.includeDir + "/libz_static")
    ]
    ++ base.includeDirs;
}