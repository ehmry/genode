/*
 * \brief  Base packages
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ tool, callComponent }:

let

  # Version string for core.
  versionObject =
    let version = builtins.readFile ../../VERSION; in
    tool.compileCC {
      src = ./src/core/version.cc;
      extraFlags = [ ''-DGENODE_VERSION="\"${version}\""'' ];
    };

  compileCC =
  attrs:
  tool.compileCC (attrs // {
    systemIncludes =
     (attrs.systemIncludes or [])
      ++
      (import ./include { inherit (tool) genodeEnv; });
  });

  callComponent' = callComponent {
    inherit (tool) genodeEnv; inherit compileCC;
  };

  importComponent = path: callComponent' (import path);

  baseDir = ../base;
  repoDir =
    if tool.genodeEnv.isLinux then ../base-linux else
    if tool.genodeEnv.isNova  then ../base-nova  else
    throw "no base components for ${tool.genodeEnv.system}";

  callBasePackage = callComponent {
    inherit (tool) genodeEnv;
    inherit compileCC baseDir repoDir versionObject;
  };
  importBaseComponent = path: callBasePackage (import path);

  impl =
    import (repoDir+"/pkgs.nix") { inherit importBaseComponent; };

in
impl // {
  test =
    { sub_rm = importBaseComponent ./src/test/sub_rm; } //
    # No boilerplate for test expressions.
    (builtins.listToAttrs (map
      (dir:
        { name = dir;
          value = callComponent' (
            { base }:
            tool.genodeEnv.mkComponent {
              name = "test-"+dir;
              libs = [ base ];
              objects = compileCC {
                src = ./src/test + "/${dir}/main.cc";
              };
           });
        }
      )
      (builtins.filter (x: !(builtins.elem x [ "ada" "sub_rm" ]))
        (builtins.attrNames (builtins.readDir ./src/test))
      )
    )) //
    impl.test or {};
}
