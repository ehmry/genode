{ genodeEnv, linkStaticLibrary, compileLibc, fromLibc }:

linkStaticLibrary {
  name = "libc-setjmp";
  externalObjects = compileLibc {
    sources =
      if genodeEnv.isx86_32 then
        [ "lib/libc/i386/gen/_setjmp.S" "lib/libc/i386/gen/setjmp.S" ]
      else
      if genodeEnv.isx86_64 then
        [ "lib/libc/amd64/gen/_setjmp.S" "lib/libc/amd64/gen/setjmp.S" ]
      else throw "incomplete libc-setjmp expression for ${genodeEnv.system}";
      externalIncludes = fromLibc [ "sys" ];
   };
}
