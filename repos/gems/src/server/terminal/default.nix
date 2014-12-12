{ linkComponent, compileCC, transformBinary, base, config }:

linkComponent {
  name = "terminal";
  libs = [ base config ];
  objects =
    [ (compileCC { src = ./main.cc; }) ] ++
    ( map
        transformBinary
        [ ./notix-8.tff ./terminus-12.tff  ./terminus-16.tff ]
    );
}
