{
  description = "Genode development flake";

  edition = 201909;

  inputs.genodepkgs.uri = "git+https://git.sr.ht/~ehmry/genodepkgs?ref=staging";

  outputs = { self, nixpkgs, genodepkgs }: {

    devShell.x86_64-linux =
      genodepkgs.packages.x86_64-linux-x86_64-genode.genode.base;

  };
}
