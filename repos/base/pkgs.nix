/*
 * \brief  Base packages
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ spec, tool, callComponent }:

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
      (import ./include { inherit spec; inherit (tool) filterHeaders; });
  });

  callComponent' = callComponent {
    inherit compileCC;
  };

  importComponent = path: callComponent' (import path);

  baseDir = ../base;
  repoDir =
    if spec.isFiasco then ../base-fiasco else
    if spec.isLinux  then ../base-linux  else
    if spec.isNova   then ../base-nova   else
    throw "no base components for ${spec.system}";

  callBasePackage = callComponent {
    inherit compileCC baseDir repoDir versionObject coreRuntime;
  };
  importBaseComponent = path: callBasePackage (import path);

  impl =
    import (repoDir+"/pkgs.nix") { inherit importBaseComponent; };

  coreRuntime.provides =
    [ "CAP" "CPU" "LOG" "IO_MEM"
      "IO_PORT" "IRQ" "PD" "RAM"
      "RM" "ROM" "SIGNAL" "TRACE"
    ];

in
impl // {
  test =
    # No boilerplate for test expressions.
    (builtins.listToAttrs (map
      (dir:
        let default = ./src/test + "/${dir}/default.nix"; in
        { name = dir;
          value =
            if builtins.pathExists default then importBaseComponent default
            else callComponent' (
              { base }:
              tool.linkStaticComponent {
                name = "test-"+dir;
                libs = [ base ];
                objects = compileCC {
                  src = ./src/test + "/${dir}/main.cc";
                };
             });
        }
      )
      (builtins.filter
        (x: !builtins.elem x [ "ada" ])
        (builtins.attrNames (builtins.readDir ./src/test))
      )
    )) //
    impl.test or {};
}
