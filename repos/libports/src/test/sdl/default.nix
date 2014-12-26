{ linkComponent, compileCC, sdl, libc }:

linkComponent rec {
  name = "test-sdl";
  libs = [ sdl libc ];
  objects = [ (compileCC { src = ./main.cc; inherit libs;}) ];
}
