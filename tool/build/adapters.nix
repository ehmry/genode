/*
 * \brief  Functions that modify genodeEnv
 * \author Emery Hemingway
 * \date   2014-10-23
 */

{

  ##
  # Add system (<...>) include paths to build.
  #
  addSystemIncludes = genodeEnv: includes:
    genodeEnv //
      { mkLibrary = args: genodeEnv.mkLibrary (args // {
          systemIncludes = (args.systemIncludes or []) ++ includes;
        });
        
        mkComponent = args: genodeEnv.mkComponent (args // {
          systemIncludes = (args.systemIncludes or []) ++ includes;
        });
      };

  ##
  # Set the dynamic linker library.
  # TODO: find a better way.
  addDynamicLinker = genodeEnv: dynamicLinker:
    genodeEnv //
      { mkComponent = args: genodeEnv.mkComponent (
          { inherit dynamicLinker; } // args
        );
      };

}
