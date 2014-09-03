{ build, base }:

build.test {
  name = "test-lx_hybrid_errno";
  libs = [ base.lib.lx_hybrid ];
  sources = [ ./main.cc ];
  includeDirs = base.includeDirs;
}