{
  edition = 201909;

  description = "Genode system flake";

  inputs.nixpkgs.uri = "git+https://gitea.c3d2.de/ehmry/nixpkgs.git?ref=genode";

  outputs = { self, nixpkgs }: {
    packages = nixpkgs.lib.forAllCrossSystems
      ({ system, localSystem, crossSystem }:
        import ./default.nix {
          inherit localSystem crossSystem self;
          nixpkgs = builtins.getAttr system nixpkgs.legacyPackages;
        });

    defaultPackage.x86_64-linux = self.packages.x86_64-linux-x86_64-genode.os;

    hydraJobs = self.packages;
  };
}
