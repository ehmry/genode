{ genodeEnv, compileCC }:

genodeEnv.mkLibrary {
  name = "scout_gfx";
  objects = compileCC { src = ./sky_texture_painter.cc; };
}
