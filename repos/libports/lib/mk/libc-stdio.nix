{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-stdio"; srcSh = [ "lib/libc/stdio/*.c" ];
}