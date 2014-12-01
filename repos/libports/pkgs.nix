/*
 * \brief  Libports components
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ tool, callComponent }:

let

  ld = callComponent {} ({ld}: ld);

  ports = import ./ports { inherit tool; };

  includes =
    [ #(ports.libc + "/include/libc")
      #(ports.libc + "/include/libc-amd64")
      ./include/libc-genode
    ];

  importInclude = p: import p { inherit (tool) genodeEnv; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
     (importInclude ../base/include) ++
     (importInclude ./include) ++
     includes;
  });

  # Prepare genodeEnv.
  genodeEnv' = tool.genodeEnvAdapters.addSystemIncludes
    tool.genodeEnv (
      ( import ../base/include { inherit (tool) genodeEnv; }) ++
      includes
    );

  genodeEnv = tool.genodeEnvAdapters.addDynamicLinker
    genodeEnv' "${ld}/ld.lib.so";

  callComponent' = callComponent { inherit genodeEnv compileCC; };
  importComponent = path: callComponent' (import path);

in
{
  driver.framebuffer = importComponent ./src/drivers/framebuffer/vesa;

  test =
    { ldso = importComponent ./src/test/ldso;
      libc = importComponent ./src/test/libc;
    };
}
