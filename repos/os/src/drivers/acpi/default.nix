{ genodeEnv, compileCC, base }:

if genodeEnv.isx86 then genodeEnv.mkComponent {
  name = "acpi_drv";
  libs = [ base ];
  objects = map
    (src: compileCC { inherit src; })
    [ ./main.cc ./acpi.cc ];
} else null
