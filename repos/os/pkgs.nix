/*
 * \brief  OS components
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ tool, callComponent }:

let

  importInclude = p: import p { inherit (tool) genodeEnv; };

  compileCC = attrs:
    tool.compileCC (attrs // {
      systemIncludes =
       (attrs.systemIncludes or []) ++
       (importInclude ../base/include) ++
       (importInclude ./include);
    });

  callComponent' = callComponent {
    inherit (tool) genodeEnv transformBinary;
    inherit compileCC;
  };
  importComponent = path: callComponent' (import path);

in
{
  app =
    { pointer    = importComponent ./src/app/pointer;
      status_bar = importComponent ./src/app/status_bar;
    };

  driver =
    { acpi          = importComponent ./src/drivers/acpi;
      ahci          = importComponent ./src/drivers/ahci;
      input =
        { ps2       = importComponent ./src/drivers/input/ps2; };
      pci           = importComponent ./src/drivers/pci;
      pci_device_pd = importComponent ./src/drivers/pci/device_pd;
      nic           = importComponent ./src/drivers/nic/lan9118;
      rtc           = importComponent ./src/drivers/rtc;
      timer         = importComponent ./src/drivers/timer;
    };

  server =
    { loader       = importComponent ./src/server/loader;
      log_terminal = importComponent ./src/server/log_terminal;
      nitpicker    = importComponent ./src/server/nitpicker;
      report_rom   = importComponent ./src/server/report_rom;
      ram_fs       = importComponent ./src/server/ram_fs;
      tar_fs       = importComponent ./src/server/tar_fs;
      tar_rom      = importComponent ./src/server/tar_rom;
    };

  test =
    { alarm       = importComponent ./src/test/alarm;
      audio_out   = importComponent ./src/test/audio_out;
      bomb        = importComponent ./src/test/bomb;
      #framebuffer = importComponent ./src/test/framebuffer;
      loader      = importComponent ./src/test/loader;
      input       = importComponent ./src/test/input;
      nitpicker   = importComponent ./src/test/nitpicker;
      pci         = importComponent ./src/test/pci;
      signal      = importComponent ./src/test/signal;
      testnit     = importComponent ./src/test/nitpicker;
      timer       = importComponent ./src/test/timer;
      xml_generator = importComponent ./src/test/xml_generator;
    };

  init = importComponent ./src/init;
}
