/*
 * \brief  Libports components
 * \author Emery Hemingway
 * \date   2014-09-21
 */

{ tool, callComponent }:

let

  ports = import ./ports { inherit tool; };

  includes =
    [ #(ports.libc + "/include/libc")
      #(ports.libc + "/include/libc-amd64")
      ./include/libc-genode
    ];

  # Prepare genodeEnv.
  genodeEnv' =  tool.genodeEnvAdapters.addSystemIncludes
    tool.genodeEnv (
      ( import ../base/include { inherit (tool) genodeEnv; }) ++
      includes
    );

  callComponent' = callComponent { genodeEnv = genodeEnv'; };
  importComponent = path: callComponent' (import path);

in
{
  test =
    { ldso = importComponent ./src/test/ldso;
      libc = importComponent ./src/test/libc;
    };
}
