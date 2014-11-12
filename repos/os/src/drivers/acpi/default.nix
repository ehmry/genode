{ genodeEnv, base }:

if genodeEnv.isx86 then genodeEnv.mkComponent {
  name = "acpi_drv";
  libs = [ base ];
  sources = genodeEnv.fromPaths [ ./main.cc ./acpi.cc ];
} else null
