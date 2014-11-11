{ subLibcEnv }:

subLibcEnv.mkLibrary {
  name = "libc-locale"; sourceSh = [ "lib/libc/locale/*.c" ];
}
