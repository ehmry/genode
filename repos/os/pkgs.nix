/*
 * \brief  OS components.
 * \author Emery Hemingway
 * \date   2014-09-15
 */

 { build, callPackage, baseIncludes, osIncludes }:

let

  # overide the build.component function
  build' = build // {
    component = { includeDirs ? [], ... } @ args:
      build.component (args // {
        includeDirs =  builtins.concatLists [
          includeDirs osIncludes baseIncludes
        ];
      });
  };
  
  os = rec {
    sourceDir = ./src;
    includeDir = ./include;
    includeDirs = osIncludes;
  };

  importComponent = path:
    callPackage (import path { build = build'; });

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