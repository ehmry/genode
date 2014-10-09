{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-regex";
  srcSh = [ "lib/libc/regex/*.c" ];
  filter =
    [ # 'engine.c' is meant to be included by other compilation
      # units. It cannot be compiled individually.
      "lib/libc/regex/engine.c"
    ];
  incDir = [ "lib/libc/regex" ];
}
