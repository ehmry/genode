# The Nix conversion

If you would like to try it out, here is few places to start:

```
$ nix-repl default.nix

nix-repl> :?
nix-repl> :b pkgs.init
nix-repl> :b pkgs.app.launchpad
nix-repl> run.fpu
nix-repl> :b run.fpu
nix-repl> :q

$ nix-build -A run.thread
$ nix-build repos/libports/ports -A libc
$ nix-build repos/ports/ports -A dosbox
```

I've been rebasing and squashing and forcing pushes to the Github repo to
get Hydra working just right, so I apologize if that makes it hard to follow.

Booting NOVA tests was working previously, but I'm in the process of reorganizing
functions for building libraries and components, when that is finished, then I
get back to system building and booting.

I started with everything in a single namespace broken up by repo, like base.core,
 os.init, demo.app.scout, but now everything is in libs, pkgs, test, and run.
Only pkgs and run are exported in the 'default.nix' entry expression.

When I say entry expression I mean files that can be loaded directly by nix-build.
Currently the only real entry expressions are the top-level default.nix and the port
directories in the libports and ports repos.

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
