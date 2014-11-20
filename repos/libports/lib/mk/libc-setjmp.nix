{ subLibcEnv }:
subLibcEnv.mkLibrary {
  name = "libc-setjmp";
  sourceSh =
    if subLibcEnv.isx86_32 then
      [ "lib/libc/i386/gen/_setjmp.S" "lib/libc/i386/gen/setjmp.S" ]
    else
    if subLibcEnv.isx86_64 then
      [ "lib/libc/amd64/gen/_setjmp.S" "lib/libc/amd64/gen/setjmp.S" ]
    else throw "incomplete libc-setjmp expression for ${subLibcEnv.system}";
}
