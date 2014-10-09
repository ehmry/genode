{ subLibcEnv, libcSrc }:

subLibcEnv.mkLibrary {
  name = "libc-compat"; srcSh = [ "lib/libc/compat-43/*.c" ];
}
