{ genodeEnv, compileCC }:

genodeEnv.mkLibrary {
  name = "scout_gfx";
  object = compileCC { src = ./sky_texture_painter.cc; };
}
