/*
* \brief  Expression for the base repo
* \author Emery Hemingway
* \date   2014-08-11
*
* Here the plaform implementation is imported in local scope
* and combined with the final attribute set.
*
* Implentations should only be imported through here to keep
* a clean interface.
*
*/

{ tool, os }:

let
  inherit (tool) build;

  impl =
    if build.spec.kernel == "linux" then import ../base-linux { inherit tool base; } else
    if build.spec.kernel == "nova"  then import ../base-nova  { inherit tool base; } else
    throw "no base support for ${build.spec}";

  ## TODO: get the version, look at NixOS for a reference
  version = builtins.readFile ../../VERSION;

  buildLibFromBase =
    { name, includeDirs ? [] }:
    let x = builtins.getAttr name impl.lib; in
    build.library
      ( x
        //
        { inherit name;
          shared = false;
          sources = x.sources or [ ];
          includeDirs = x.includeDirs ++ includeDirs;
        }
      );

  base = rec {
    sourceDir = ./src;
    includeDir = ./include;
    includeDirs =
      impl.includeDirs ++
      [ ( if build.is32Bit then includeDir + "/32bit" else
          if build.is64Bit then includeDir + "/64bit" else
            abort "platform is not known to be 32 or 64 bit"
        )
        ( if build.isx86 then includeDir + "/x86" else
          if build.isArm then includeDir + "/arm" else
            abort "unknown cpu for platform"
        )
        ( if build.isx86_32 then includeDir + "/x86_32" else
          if build.isx86_64 then includeDir + "/x86_64" else
            abort "unknown cpu for platform"
        )
        includeDir
        "${build.toolchain}/lib/gcc/${build.spec.target}/${build.toolchain.version}/include"
        "${build.toolchain.glibc}/include"
      ];

    lib = impl.lib // {

      base = buildLibFromBase
        { name = "base";
          inherit includeDirs;
        };

      base-common = buildLibFromBase
        { name = "base-common";
          includeDirs =
            map (d: base.sourceDir + "/base/${d}") [ "env" "lock" "thread" ]
            ++ base.includeDirs;
        };

      cxx = import ./lib/mk/cxx.nix { inherit build base; };
      startup = import ./lib/mk/startup.nix { inherit build base impl;};

      syscall = buildLibFromBase { name = "syscall"; };

    };
  
    core = build.program
      ( impl.core
        //
        { name = "core";
          includeDirs =
            impl.core.includeDirs
            ++ [ (base.sourceDir + "/core/include") ]
            ++ map (d: base.sourceDir + "/base/${d}") [ "env" "thread" ]
            ++ base.includeDirs;
          flags = (impl.core.flags or []) ++ [ "-DGENODE_VERSION='\"${version}\"'" ];
        }
      );

    test = {
      affinity = import ./src/test/affinity { inherit build base; };
      fpu      = import ./src/test/fpu      { inherit build base; };
      thread   = import ./src/test/thread   { inherit build base; };
    };

  };
in
base // {

  merge =
  { base, ... }:
  let
    importRun = p: import p { inherit tool base os; };
  in
  {
    inherit (base) lib core;

    test = base.test // impl.test or {};

    run = {
      affinity = if build.spec.kernel == "linux" then null else importRun ./run/affinity.nix;
      fpu    = importRun ./run/fpu.nix;
      thread = importRun ./run/thread.nix;
    } // impl.run or {};

  } // impl.final;

}
