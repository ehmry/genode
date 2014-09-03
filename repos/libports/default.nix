/*
 * \brief  Expression for the libports repo.
 * \author Emery Hemingway
 * \date   2014-08-19
 */

{ tool, base, os }:

let
  inherit (tool) build;

  preparePort = import ./tool/prepare-port { inherit (tool) nixpkgs; };

  importComponent = p: import p { inherit build libports; };

  libports = {

    includeDir = ./include;

    port = {
      libc   = import ./ports/libc.nix   { inherit tool preparePort; };
      x86emu = import ./ports/x86emu.nix { inherit tool preparePort; };
    };

    lib =
      { #libc = import ./src/lib/libc { inherit build base libports; };
        x86emu = import ./lib/mk/x86emu.nix { inherit build base os libports; };
      };

    driver =
      { framebuffer = 
          { vesa = import ./src/drivers/framebuffer/vesa
              { inherit build base os libports; };
          };
      };

  };
in
libports // {

  merge =
  { base, os, libports, ... }:
  {
    inherit (libports) lib driver;

    #test =
    #  { sdl = importComponent ./src/test/sdl;
    #  };
  };

}