{ build, base, os, demo }:

build.program {
  name = "scout";
  libs = with demo.lib; [
    launchpad scout_widgets
    libpng_static
  ];

  sources =
    [ ./main.cc
      ./about.cc ./browser_window.cc ./doc.cc
      ./launcher.cc ./navbar.cc ./png_image.cc
    ];

  binaries =
    map (fn: (./data + "/${fn}"))
      [ "ior.map"
        "cover.rgba"
        "forward.rgba"
        "backward.rgba"
        "home.rgba"
        "index.rgba"
        "about.rgba"
        "pointer.rgba"
        "nav_next.rgba"
        "nav_prev.rgba"
      ]
    ++
    map (fn: (../../../doc/img + "/${fn}"))
      [ "genode_logo.png"
        "launchpad.png"
        "liquid_fb_small.png"
        "setup.png"
        "x-ray_small.png"
      ];

  flags = [ "-DPNG_USER_CONFIG" ];

  includeDirs =
   [ ../scout 
     demo.includeDir
     (demo.includeDir + "/libpng_static")
     (demo.includeDir + "/libz_static")
     os.includeDir
   ]
   ++ base.includeDirs;
}