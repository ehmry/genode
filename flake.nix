{
  edition = 201909;

  description = "Genode system flake";

  inputs = {
    nixpkgs.uri = "git+https://gitea.c3d2.de/ehmry/nixpkgs.git?ref=genode";
    dhall-haskell.uri =
      "git+https://github.com/dhall-lang/dhall-haskell?ref=flake";
  };

  outputs = { self, nixpkgs, dhall-haskell }:
    let
      mkOutput = { system, localSystem, crossSystem }:
        import ./default.nix {
          inherit localSystem crossSystem self;
          nixpkgs = builtins.getAttr system nixpkgs.legacyPackages;
          inherit dhall-haskell;
        };

      localSystems = [ "x86_64-linux" ];
      crossSystems = [ "x86_64-genode" ];

      forAllCrossSystems = f:
        with builtins;
        let
          f' = localSystem: crossSystem:
            let system = localSystem + "-" + crossSystem;
            in {
              name = system;
              value = f { inherit system localSystem crossSystem; };
            };
          list = nixpkgs.lib.lists.crossLists f' [ localSystems crossSystems ];
          attrSet = listToAttrs list;
        in attrSet;

      finalize = outputs:
        with builtins;
        let
          outputs' = outputs // {
            x86_64-linux = getAttr "x86_64-linux-x86_64-genode" outputs;
          };
          systems = attrNames outputs';
          outputAttrs = attrNames (head (attrValues outputs'));

          list = map (attr: {
            name = attr;
            value = listToAttrs (map (system: {
              name = system;
              value = getAttr attr (getAttr system outputs');
            }) systems);
          }) outputAttrs;

        in listToAttrs list;

      final = finalize (forAllCrossSystems mkOutput);

    in final;

}
