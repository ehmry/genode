{ genodeEnv, linkComponent, compileCC, base }:

if genodeEnv.isx86 then linkComponent {
  name = "acpi_drv";
  libs = [ base ];
  objects = map
    (src: compileCC { inherit src; })
    [ ./main.cc ./acpi.cc ];
} else null
