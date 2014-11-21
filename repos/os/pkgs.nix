/*
 * \brief  OS components
 * \author Emery Hemingway
 * \date   2014-09-15
 */

 { tool, callComponent }:

let

  importInclude = p: import p { inherit (tool) genodeEnv; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
     (importInclude ../base/include) ++
     (import ./include { inherit (tool) genodeEnv; });
  });

  callComponent' = callComponent {
    inherit (tool) genodeEnv transformBinary;
    inherit compileCC;
  };
  importComponent = path: callComponent' (import path);

in
{
  driver =
    { acpi          = importComponent ./src/drivers/acpi;
      input =
        { ps2       = importComponent ./src/drivers/input/ps2; };
      pci           = importComponent ./src/drivers/pci;
      pci_device_pd = importComponent ./src/drivers/pci/device_pd;
      timer         = importComponent ./src/drivers/timer;
    };

  server = { nitpicker = importComponent ./src/server/nitpicker; };

  test =
    { alarm   = importComponent ./src/test/alarm;
      bomb    = importComponent ./src/test/bomb;
      input   = importComponent ./src/test/input;
      pci     = importComponent ./src/test/pci;
      signal  = importComponent ./src/test/signal;
      testnit = importComponent ./src/test/nitpicker;
      timer   = importComponent ./src/test/timer;
    };

  init = importComponent ./src/init;

}
