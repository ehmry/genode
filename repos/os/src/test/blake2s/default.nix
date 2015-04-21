{ linkComponent, compileCC, base, blake2s }:

linkComponent {
  name = "test-blake2s";
  libs = [ base blake2s ];
  objects = compileCC {
    src = ./main.cc;
    # This header is too big to process for includes.
    extraFlags = [ "-include ${./blake2-kat.h}" ];
  };
}