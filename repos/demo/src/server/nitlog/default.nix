{ linkComponent, compileCC, transformBinary, base, blit }:

linkComponent {
  name = "nitlog";
  libs = [ base blit ];
  objects =
    [ (compileCC { src = ./main.cc;})
      (transformBinary ./mono.tff)
    ];
}
