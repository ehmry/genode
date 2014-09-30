{ tool }: with tool;
{ }: # no library dependencies

buildLibrary {
  name = "blit";
  sources = [ ./blit.cc ];
  includeDirs = 
    if tool.build.isArm then [ ./arm ] else
    if tool.build.isx86_32 then [ ./x86 ./x86/x86_32 ] else
    if tool.build.isx86_64 then [ ./x86 ./x86/x86_64 ] else
    throw "no blit library for build.system";
}