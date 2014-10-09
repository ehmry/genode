{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-setjmp";
  srcSh =
    if subLibcEnv.isx86_64 then
      [ "lib/libc/amd64/gen/_setjmp.S" "lib/libc/amd64/gen/setjmp.S" ]
    else throw "no libc-setjmp for ${subLibcEnv.system}";
}
