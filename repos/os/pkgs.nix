/*
 * \brief  OS components
 * \author Emery Hemingway
 * \date   2014-09-15
 */

 { tool, callComponent }:

let

  # Prepare genodeEnv.
  genodeEnv =  tool.genodeEnvAdapters.addSystemIncludes
    tool.genodeEnv (
      ( import ../base/include { inherit (tool) genodeEnv; }) ++
      [ ./include ]);

  callComponent' = callComponent { inherit genodeEnv; };
  importComponent = path: callComponent' (import path);

in
{
  driver = {
    acpi          = importComponent ./src/drivers/acpi;
    input = {
      ps2         = importComponent ./src/drivers/input/ps2;
    };
    pci           = importComponent ./src/drivers/pci;
    pci_device_pd = importComponent ./src/drivers/pci/device_pd;
    timer         = importComponent ./src/drivers/timer;
  };

  server = {
    nitpicker = importComponent ./src/server/nitpicker;
  };

  test = {
    signal = importComponent ./src/test/signal;
    timer  = importComponent ./src/test/timer;
  };

  init = importComponent ./src/init;

}
