{ genodeEnv, compileCC, transformBinary, base, blit }:

genodeEnv.mkComponent {
  name = "nitlog";
  libs = [ base blit ];
  objects =
    [ (compileCC { src = ./main.cc;})
      (transformBinary ./mono.tff)
    ];
}
