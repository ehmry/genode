{ tool, pkgs, ... }:

{
  platformFunc = { name }: { inherit (pkgs) core init; };
  platformBuild = ./linux-build.exp;
  platformSetup = ./linux-setup.exp;
}
