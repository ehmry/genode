{ spec, linkComponent, compileCC, toolchain
, base, config, server, lx_hybrid }:

if !spec.isLinux then null else
linkComponent {
  name = "lx_fs";
  libs = [ base config server lx_hybrid ];
  objects = compileCC {
    src = ./main.cc;
    includes = [ ../lx_fs ];
    externalIncludes = [ "${toolchain.libc}/include" ];
  };
}