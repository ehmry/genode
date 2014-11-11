{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-inet"; sourceSh = [ "lib/libc/inet/*.c" ];
}
