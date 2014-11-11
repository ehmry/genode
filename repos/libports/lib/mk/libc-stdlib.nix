{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-stdlib";
  sourceSh = [ "lib/libc/stdlib/*.c" ];
  filter = map
    (fn: "lib/libc/stdlib/${fn}") [ "exit.c" "atexit.c" "malloc.c" ];
}