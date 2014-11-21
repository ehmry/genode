{ genodeEnv, compileCC, transformBinary, base, blit, scout_gfx }:

let
  compileCC' = src: compileCC {
    inherit src; systemIncludes = [ ../scout ];
  };
in
genodeEnv.mkLibrary {
  name = "scout_widgets";
  libs = [ base blit scout_gfx ];
  objects =
    (map (src: compileCC { inherit src; })
      [ ./elements.cc ./scrollbar.cc ./tick.cc ./widgets.cc ]
    ) ++
    ( map (fn: transformBinary (./data + "/${fn}"))
      [ "vera16.tff" "verai16.tff"
        "vera18.tff" "vera20.tff"
        "vera24.tff" "verabi10.tff"
        "mono16.tff"

        "uparrow.rgba" "downarrow.rgba"
        "slider.rgba" "sizer.rgba"
        "titlebar.rgba" "loadbar.rgba"
        "redbar.rgba" "whitebar.rgba"
        "kill_icon.rgba" "opened_icon.rgba" "closed_icon.rgba"
      ]
    );
}
