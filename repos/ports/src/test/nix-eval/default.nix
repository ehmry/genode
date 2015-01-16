{ linkComponent, compileCC, libc, libm, stdcxx, libnixexpr, libnixstore, libnixutil, base }:

linkComponent rec {
  name = "nix-eval";
  libs = [ libc libm stdcxx libnixexpr libnixstore libnixutil ];
  objects = compileCC {
    src = ./main.cc;
    libs = [ base ] ++ libs;

    # Make sure libc's regex.h comes before stdcxx's regex.h.
    extraFlags = map (i: "-I"+i) libc.propagate.systemIncludes;
  };
}