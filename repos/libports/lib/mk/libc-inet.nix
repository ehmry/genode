{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-inet"; srcSh = [ "lib/libc/inet/*.c" ];
}
