{ subLibcEnv }:

subLibcEnv.mkLibrary {
  name = "libc-locale"; srcSh = [ "lib/libc/locale/*.c" ];
}
