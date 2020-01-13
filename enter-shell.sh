#! /bin/sh
set -e

nix build .#packages.x86_64-linux-x86_64-genode.tupConfigGcc -o build-gcc/tup.config
export SHELL=bash
exec nix dev-shell
