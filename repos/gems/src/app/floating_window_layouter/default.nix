{ linkComponent, compileCC, base, cxx }:

linkComponent {
  name = "floating_window_layouter";
  libs = [ base cxx ];
  objects = compileCC {
    src = ./main.cc;
  };
}

