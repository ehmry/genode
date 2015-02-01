{ linkStaticLibrary, compileCRepo, compileCC, jitterentropySrc, filterHeaders, spec }:

let
  extraFlags = [ "-DJITTERENTROPY_GENODE" ];
  systemIncludes =
    [ jitterentropySrc.include
      (filterHeaders ../jitterentropy)
      ( if spec.isArmV6 then ./arm_v6 else
        if spec.isArmV7 then ./arm_v7 else
        if spec.isx86_32 then ./x86_32 else
        if spec.isx86_64 then ./x86_64 else
        throw "jitterentropy not expressed for ${spec.system}"
      )
    ];
in
linkStaticLibrary {
  name = "jitterentropy";
  externalObjects = compileCRepo {
    inherit extraFlags systemIncludes;
    sourceRoot = jitterentropySrc;
    sources = "jitterentropy-base.c";
    optimization = "-O0";
  };
  objects = compileCC {
    inherit extraFlags systemIncludes;
    src = ./jitterentropy-base-genode.cc;
  };
  propagate = { inherit extraFlags systemIncludes; };
}