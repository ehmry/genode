{ tool, ... }:

{
  platformFunc =
    { name, contents }:
    { systemImage = tool.systemImage { inherit name contents; }; };


  platformBuild = ./linux-build.exp;
  platformSetup = ./linux-setup.exp;
}
