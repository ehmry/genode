{ linkComponent, compileCC, transformBinary, base }:

linkComponent {
  name = "status_bar";
  libs = [ base ];
  objects =
    [ (compileCC { src = ./main.cc; })
      (transformBinary ../../server/nitpicker/default.tff)
    ];
}
