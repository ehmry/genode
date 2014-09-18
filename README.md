# The Nix conversion

This my Nix conversion of the Genode build system.

It doesn't work as well as it used because I've been doing alot of re-arranging.

If you would like to try it out, here is few places to start:

```
$ nix-repl <GENODE_DIR>/default.nix

nix-repl> :?
nix-repl> :b pkgs.init
nix-repl> :b pkgs.app.launchpad
nix-repl> run.fpu
nix-repl> :b run.fpu
nix-repl> :q

$ nix-build <GENODE_DIR>/default.nix -A run.thread
```

I've been rebasing and squashing and forcing pushes to the Github repo to
get Hydra working just right, so I apologize if it makes it hard to follow.

I started with everything in a single namespace broken up by repo,
like base.core, os.init, demo.app.scout, but now everything is in
libs, pkgs, test, and run. Only pkgs and run are exported in the 'default.nix'
entry expression, as libs and test are not usefull in and of themselves (thats
not entirely true of test but I'll deal with that another way).

## Todo
- Append git revision info to version string.
- Strip compilation and link outputs so that source code and intermediate
  object files don't get interpreted as runtime dependencies.
- Find a better way to handle include directories
- Generate GRUB2 configs.
- Adapt the NixOS module framework, but in a nested rather than flat fashion.
- Get Noux working.
- Port Nix to Noux.
- Devise a system of expressing the runtime service dependencies of components
  so that Nix can automatically build a running system by requesting only a few
  high-level applications.
