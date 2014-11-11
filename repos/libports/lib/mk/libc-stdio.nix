{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-stdio"; sourceSh = [ "lib/libc/stdio/*.c" ];
}