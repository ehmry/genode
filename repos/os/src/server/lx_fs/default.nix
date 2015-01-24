{ spec, linkComponent, compileCC, filterHeaders, toolchain
, base, config, server, lx_hybrid }:

if !spec.isLinux then null else
linkComponent {
  name = "lx_fs";
  libs = [ base config server lx_hybrid ];
  objects = compileCC {
    src = ./main.cc;
    systemIncludes = 
      [ (filterHeaders ../lx_fs)
        "${toolchain.libc}/include"
      ];
  };
}