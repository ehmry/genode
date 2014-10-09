{ tool }:
{ base, blit, scout_gfx }:

with tool;

buildLibrary {
  name = "scout_widgets";
  libs = [ base blit scout_gfx ];
  sources = [ ./elements.cc ./scrollbar.cc ./tick.cc ./widgets.cc ];
  binaries = fromDir ./data
    [ "vera16.tff" "verai16.tff"
      "vera18.tff" "vera20.tff"
      "vera24.tff" "verabi10.tff"
      "mono16.tff"

      "uparrow.rgba" "downarrow.rgba"
      "slider.rgba" "sizer.rgba"
      "titlebar.rgba" "loadbar.rgba"
      "redbar.rgba" "whitebar.rgba"
      "kill_icon.rgba" "opened_icon.rgba" "closed_icon.rgba"
    ];
  includeDirs = [ ../scout ];
}