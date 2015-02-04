{ linkStaticLibrary, compileCC, spec, base }:

linkStaticLibrary {
  name = "ahci";
  libs = [ base ];
  objects =
    [( compileCC {
         src = ./main.cc;
         includes =
           [ ( if spec.isx86 then ./x86
               else throw "no AHCI library expression for ${spec.system}"
             )
             ./include
           ];
       }
    )];
}
