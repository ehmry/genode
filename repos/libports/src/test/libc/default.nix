{ build }:

{ libm libc }:

build.component {
  name = "test-libc";
  sources = [ ./main.cc ];
  libs = [ libm libc ];
}