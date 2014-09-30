{ tool }: with tool;
{ libm, libc, ld }:

buildComponent {
  name = "test-libc";
  sources = [ ./main.cc ];
  libs = [ libm libc ld ];
}