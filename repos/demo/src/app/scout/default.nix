{ linkComponent, compileCC, transformBinary
, launchpad, scout_widgets, libpng_static }:

let
  demoInclude = ../../../include;

  compileCC' = src: compileCC {
    inherit src;
    extraFlags = [ "-DPNG_USER_CONFIG" ];

    systemIncludes =
      [ ../../../include/libpng_static
        ../../../include/libz_static
        ../../../include/mini_c
      ];
  # [ ../scout
  #   (demoInclude + "/libpng_static")
  #
  # ];
  };
in
linkComponent {
  name = "scout";
  libs = [ launchpad scout_widgets libpng_static ];

  objects =
    ( map compileCC'
        [ ./main.cc
          ./about.cc ./browser_window.cc ./doc.cc
          ./launcher.cc ./navbar.cc ./png_image.cc
        ]
    ) ++
    ( map transformBinary
      (  ( map (fn: (./data + "/${fn}"))
            [ "ior.map" "cover.rgba" "forward.rgba" "backward.rgba"
              "home.rgba" "index.rgba" "about.rgba" "pointer.rgba"
              "nav_next.rgba" "nav_prev.rgba"
            ]
        ) ++
        ( map (fn: (../../../doc/img + "/${fn}"))
           [ "genode_logo.png" "launchpad.png" "liquid_fb_small.png"
             "setup.png" "x-ray_small.png"
           ]
        )
      )
    );

}
