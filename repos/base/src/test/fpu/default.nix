{ tool }: with tool;
{ base }:

buildComponent {
  name = "test-fpu";
  libs = [ base ];
  sources = [ ./main.cc ];
}