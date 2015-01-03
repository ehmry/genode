{ tool, pkgs }:

{
  platformFunc =
    { name, contents }:
    {
      inherit (tool.nixpkgs) qemu;

      diskImage = tool.iso {
        inherit name contents;
        kernel = "${pkgs.hypervisor}/hypervisor";
        kernelArgs = "iommu serial";
      };
    };

  platformBuild = ./nova-build.exp;
  platformSetup = ./nova-setup.exp;
}
