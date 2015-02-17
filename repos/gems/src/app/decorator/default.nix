{ linkComponent, compileCC, transformBinary
, base, config }:

linkComponent {
  name = "decorator";
  libs = [ base config ];
  objects =
    [ (compileCC {
        src = ./main.cc;
        includes = [ ../../../../demo/include ];
      })
      (transformBinary ../../../../demo/src/app/scout/data/droidsansb10.tff)
    ];
}
