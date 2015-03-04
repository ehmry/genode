{ tool, requires, mkInitConfig, pkgs
, platformFunc, platformBuild, platformSetup }:

{ name, components, config ? null
, testScript, automatic ? true, graphical ? false }:

let
  topInit = mkInitConfig { inherit name components; };

  outstanding = requires {
    components = [ topInit ];
    inherit (pkgs.core) runtime;
  };

  runtimeLibs = components:
    let component = builtins.head components; in
    if components == [] then [] else
    (component.runtime.libs or []) ++
    runtimeLibs (builtins.tail components);

in
if outstanding != []
then throw "${name} test is missing servers for '${toString outstanding}'"
else
derivation (platformFunc { inherit name; } // {
  name = name+"-test";
  system = builtins.currentSystem;
  preferLocalBuild = true;

  builder = "${tool.nixpkgs.expect}/bin/expect";
  PATH = "${tool.nixpkgs.coreutils}/bin";
  args = [ "-nN" ./build.exp ];

  setup = ./setup.exp;

  automatic = automatic && !graphical;
  inherit graphical platformBuild platformSetup testScript;
  inherit (tool.nixpkgs) expect;

  components = components ++ (runtimeLibs components) ++ [ pkgs.libs.ld ];
  config = if config != null then config else topInit;
})
