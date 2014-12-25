{ spec, linkComponent, compileCC, base }:

if spec.isx86 then linkComponent {
  name ="rtc_drv";
  libs = [ base ];
  objects = [ (compileCC { src = ./x86/main.cc; }) ];
} else null
