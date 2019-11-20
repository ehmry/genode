{
  edition = 201909;

  description = "Genode system flake";

  inputs.nixpkgs.uri = "git+https://gitea.c3d2.de/ehmry/nixpkgs.git?ref=genode";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-genode" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
    in {
      packages = forAllSystems (system:
        import ./default.nix {
          inherit system self;
          nixpkgs = builtins.getAttr system nixpkgs.legacyPackages;
        });

      defaultPackage = forAllSystems (system: self.packages."${system}".os);
    };
}
