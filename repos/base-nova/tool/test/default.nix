{ tool, pkgs }:

{
  platformFunc = { name }:
    { inherit (tool.nixpkgs) qemu;
       inherit (pkgs) core init;
       kernel = pkgs.kernel;
       kernel_args = pkgs.kernel.args;
    };
  platformBuild = ./nova-build.exp;
  platformSetup = ./nova-setup.exp;
}
