{ tool, mkInitConfig, pkgs
, platformFunc, platformSetup, platformAuto, platformUser }:

testArgs @ { name, components, testScript, automatic ? true }:

let
  contents =
    (map
      (c: { target = "/"; source = c; })
      (components ++ [ pkgs.core pkgs.init ])
    ) ++
    [ { target = "/config";
        source = mkInitConfig { inherit name components; };
      }
    ];

in
derivation (platformFunc { inherit name contents; } // {
  name = name+"-run";
  system = builtins.currentSystem;
  preferLocalBuild = true;

  builder = "${tool.nixpkgs.expect}/bin/expect";
  args =
    [ "-nN"
      ./build.exp
      platformSetup
      platformAuto
    ];

  inherit automatic testScript;

  userScript =
    ''
      #!${tool.nixpkgs.expect}/bin/expect
      source ${platformSetup}
      source ${platformUser}

      ${testScript}
    '';
})
