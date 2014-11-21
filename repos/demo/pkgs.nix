/*
 * \brief  Demo components
 * \author Emery Hemingway
 * \date   2014-09-15
 */

{ tool, callComponent }:

let

  importInclude = p: import p { inherit (tool) genodeEnv; };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or []) ++
     (importInclude ../base/include) ++
     (import ./include { inherit (tool) genodeEnv; });
  });

  callComponent' = callComponent {
    inherit (tool) genodeEnv transformBinary;
    inherit compileCC;
  };

  importComponent = path: callComponent' (import path);

in
{
  server =
    { liquid_framebuffer =
        importComponent ./src/server/liquid_framebuffer;
      nitlog = importComponent ./src/server/nitlog;
    };

  app =
    { launchpad = importComponent ./src/app/launchpad;
      scout     = importComponent ./src/app/scout;
    };

  test =
    { libpng = importComponent ./src/lib/libpng/test.nix;
    };

}
