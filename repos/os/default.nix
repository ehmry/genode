/*
 * \brief  Expression for the OS repo.
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ tool, base }:

let
  inherit (tool) build;

  importComponent = p: import p { inherit build base os; };

  os = rec {
    sourceDir = ./src;
    includeDir = ./include;
    includeDirs =
      [ includeDir ]
      ++ (
        #if build.isARMv6 then [ ./include/arm_6 ] else
        #if build.isARMv7 then [ ./include/arm_7 ] else
        if build.isx86_32 then [ ./include/x86_32 ] else
        if build.isx86_64 then [ ./include/x86_64 ] else
        []
      );
      
    lib =
      { alarm        = importComponent ./src/lib/alarm;
        blit         = importComponent ./src/lib/blit;
        config       = importComponent ./src/lib/config;
        config_args  = importComponent ./src/lib/config_args;
        init_pd_args = importComponent ./src/lib/init_pd_args;
        server       = importComponent ./src/lib/server;
        #trace        = importComponent ./src/lib/trace;
      };

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

    init = importComponent ./src/init;

    test = {
      alarm     = importComponent ./src/test/alarm;
      bomb      = importComponent ./src/test/bomb;
      input     = importComponent ./src/test/input;
      nitpicker = importComponent ./src/test/nitpicker;
      pci       = importComponent ./src/test/pci;
      signal    = importComponent ./src/test/signal;
      timer     = importComponent ./src/test/timer;
    };

  };

in


os // {

  merge =
  { base, os, ... }:
  let
    importRun = p: import p { inherit tool base os; };
  in
  {
    inherit (os) lib init driver server test;

    run = {
      ## bomb fails with "C++ runtime: Genode::Socket_descriptor_registry<100u>::Limit_reached"
      # bomb = import ./run/bomb.nix { inherit (tool) run; inherit base os.test. };
      #demo   = importRun ./run/demo.nix;
      signal = importRun ./run/signal.nix;
      timer  = importRun ./run/timer.nix;
    };

  };

}