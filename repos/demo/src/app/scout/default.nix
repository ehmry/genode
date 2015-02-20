{ linkComponent, compileCC, transformBinary, fromDir
, launchpad, scout_widgets, libpng_static }:

linkComponent rec {
  name = "scout";
  libs = [ launchpad scout_widgets libpng_static ];

  objects =
    ( map
        (src: compileCC { inherit src libs; })
        [ ./main.cc
          ./about.cc ./browser_window.cc ./doc.cc
          ./launcher.cc ./navbar.cc ./png_image.cc
        ]
    ) ++
    ( map transformBinary
      ( (fromDir ./data
           [ "ior.map" "cover.rgba" "forward.rgba" "backward.rgba"
             "home.rgba" "index.rgba" "about.rgba" "pointer.rgba"
             "nav_next.rgba" "nav_prev.rgba"
           ]
        ) ++
        (fromDir ../../../doc/img
           [ "genode_logo.png" "launchpad.png" "liquid_fb_small.png"
             "setup.png" "x-ray_small.png"
           ]
        )
      )
    );

}
