# The Nix conversion

This my Nix conversion of the Genode build system.

It can currently build a demo ISO for NOVA without any invocation of Make,
all source to library and program building is managed by Nix.

## Todo
- Append git revision info to version string.
- Compile object files to a '.o' file in a store directory, rather than 
  directly on the output path.
- Strip compilation and link outputs so that source code and intermediate
  object files don't get interpreted as runtime dependencies.
- Find a better way to handle include directories
- Generate GRUB2 configs.
- Adapt the NixOS module framework. I have a slighty modified version of NixOS
  building boot images, but it should be less flat.
  Genode will have container-like objects, but 'init' is more heirarchical and
  nested than systemd, and the system generation should reflect that.
- Port Nix to Noux.
- Get Seoul working.

