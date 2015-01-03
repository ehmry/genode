{ tool, requires, mkInitConfig, pkgs
, platformFunc, platformBuild, platformSetup }:

{ name, components, testScript, automatic ? true, graphical ? false }:

let
  topInit = mkInitConfig { inherit name components; };

  outstanding = requires {
    components = [ topInit ];
    inherit (pkgs.core) runtime;
  };

  contents =
    (map
      (c: { target = "/"; source = c; })
      (components ++ [ pkgs.core pkgs.init ])
    ) ++
    [ { target = "/config";
        source = topInit;
      }
    ];
in
if outstanding != []
then throw "${name} test is missing servers for '${toString outstanding}'"
else
derivation (platformFunc { inherit name contents; } // {
  name = name+"-test";
  system = builtins.currentSystem;
  preferLocalBuild = true;

  builder = "${tool.nixpkgs.expect}/bin/expect";
  PATH = "${tool.nixpkgs.coreutils}/bin";
  args = [ "-nN" ./build.exp ];

  setup = ./setup.exp;

  inherit automatic graphical platformBuild platformSetup testScript;
  inherit (tool.nixpkgs) expect;
})
