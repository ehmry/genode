{ linkStaticLibrary, compileCC }:

linkStaticLibrary {
  name = "scout_gfx";
  objects = compileCC { src = ./sky_texture_painter.cc; };
}
