/*
 * \brief  Libports components
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ tool, callComponent }:

let

  ld = callComponent {} ({ld}: ld);

  importInclude = p: import p { inherit (tool) genodeEnv; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
     (importInclude ../base/include) ++
     (importInclude ./include);
  });

  callComponent' = callComponent {
    inherit (tool) genodeEnv; inherit compileCC;
  };
  importComponent = path: callComponent' (import path);

in
{
  driver.framebuffer = importComponent ./src/drivers/framebuffer/vesa;

  test =
    { ldso    = importComponent ./src/test/ldso;
      libc    = importComponent ./src/test/libc;
      libc_fs_tar_fs =
        importComponent ./src/test/libc_fs_tar_fs;
      libc_vfs = importComponent ./src/test/libc_vfs;
      pthread  = importComponent ./src/test/pthread;
      sdl      = importComponent ./src/test/sdl;
    };
}
