{ subLibcEnv, libcSrc }:

subLibcEnv.mkLibrary {
  name = "libc-compat"; sourceSh = [ "lib/libc/compat-43/*.c" ];
}
