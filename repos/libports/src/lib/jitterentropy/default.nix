{ linkStaticLibrary, compileCRepo, compileCC, jitterentropySrc, filterHeaders, spec }:

let
  headers = [ "${jitterentropySrc.include}/jitterentropy.h" ];
  extraFlags = [ "-DJITTERENTROPY_GENODE" ];
  includes =
    [ ../jitterentropy
      ( if spec.isArmV6 then ./arm_v6 else
        if spec.isArmV7 then ./arm_v7 else
        if spec.isx86_32 then ./x86_32 else
        if spec.isx86_64 then ./x86_64 else
        throw "jitterentropy not expressed for ${spec.system}"
      )
    ];
  externalIncludes = [ jitterentropySrc.include ];
in
linkStaticLibrary {
  name = "jitterentropy";
  externalObjects = compileCRepo {
    inherit headers extraFlags includes externalIncludes;
    sources = "${jitterentropySrc}/jitterentropy-base.c";
    optimization = "-O0";
  };
  objects = compileCC {
    inherit extraFlags includes externalIncludes;
    src = ./jitterentropy-base-genode.cc;
  };
  propagate = { inherit headers extraFlags includes externalIncludes; };
}