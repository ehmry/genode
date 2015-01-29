/*
 * \brief  Libports components
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ spec, tool, callComponent }:

let

  # Append 'Src' to each attribute in ports.
  ports =
    let p = import ./ports { inherit tool; }; in
    builtins.listToAttrs (map
      (n: { name = n+"Src"; value = builtins.getAttr n p; })
      (builtins.attrNames p)
  );

  importInclude = p: import p { inherit spec; inherit (tool) filterHeaders; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
     (importInclude ../base/include) ++
     [ ./include ];
  });

  importComponent = path: callComponent
    ({ inherit compileCC; } // ports)
    (import path);

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
