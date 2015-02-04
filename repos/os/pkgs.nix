/*
 * \brief  OS components
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ spec, tool, callComponent }:

let

  importInclude = p: import p { inherit spec; };

  compileCC = attrs:
    tool.compileCC (attrs // {
      includes =
       (attrs.includes or []) ++
       (importInclude ../base/include);
    });

  importComponent = path: callComponent
    { inherit compileCC; }
    (import path);

in
{
  app =
    { pointer    = importComponent ./src/app/pointer;
      status_bar = importComponent ./src/app/status_bar;
    };

  driver =
    { acpi          = importComponent ./src/drivers/acpi;
      ahci          = importComponent ./src/drivers/ahci;
      framebuffer   =
        if spec.hasSDL then importComponent ./src/drivers/framebuffer/sdl else
        null;
      input.ps2     = importComponent ./src/drivers/input/ps2;
      pci           = importComponent ./src/drivers/pci;
      pci_device_pd = importComponent ./src/drivers/pci/device_pd;
      #nic          = importComponent ./src/drivers/nic/lan9118;
      rtc           = importComponent ./src/drivers/rtc;
      timer         = importComponent ./src/drivers/timer;
    };

  server =
    { loader       = importComponent ./src/server/loader;
      log_terminal = importComponent ./src/server/log_terminal;
      lx_fs        = importComponent ./src/server/lx_fs;
      nitpicker    = importComponent ./src/server/nitpicker;
      report_rom   = importComponent ./src/server/report_rom;
      ram_blk      = importComponent ./src/server/ram_blk;
      ram_fs       = importComponent ./src/server/ram_fs;
      tar_fs       = importComponent ./src/server/tar_fs;
      tar_rom      = importComponent ./src/server/tar_rom;
    };

  test =
    { alarm       = importComponent ./src/test/alarm;
      audio_out   = importComponent ./src/test/audio_out;
      blk.cli     = importComponent ./src/test/blk/cli;
      bomb        = importComponent ./src/test/bomb;
      loader      = importComponent ./src/test/loader;
      input       = importComponent ./src/test/input;
      nitpicker   = importComponent ./src/test/nitpicker;
      pci         = importComponent ./src/test/pci;
      rtc         = importComponent ./src/test/rtc;
      signal      = importComponent ./src/test/signal;
      testnit     = importComponent ./src/test/nitpicker;
      timer       = importComponent ./src/test/timer;
      xml_generator = importComponent ./src/test/xml_generator;
    };

  init = importComponent ./src/init;
}
