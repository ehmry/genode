/*
 * \brief  Libports components
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ spec, tool, callComponent }:

let

  ld = callComponent {} ({ld}: ld);

  importInclude = p: import p { inherit spec; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
     (importInclude ../base/include) ++
     [ ./include ];
  });

  callComponent' = callComponent { inherit compileCC; };
  importComponent = path: callComponent' (import path);

in
{
  driver.framebuffer =
    if spec.hasVESA then importComponent ./src/drivers/framebuffer/vesa
    else null;

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
