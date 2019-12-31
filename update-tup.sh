#! /bin/sh
set -e

mkdir -p configs
nix build .#packages.x86_64-linux-x86_64-genode.tupConfigGcc -o configs/x86_64-gcc.config
nix build .#packages.x86_64-linux-x86_64-genode.tupConfigLlvm -o configs/x86_64-llvm.config
nix run nixpkgs#tup -c tup variant configs/x86_64-gcc.config || true
nix run nixpkgs#tup -c tup variant configs/x86_64-llvm.config || true
