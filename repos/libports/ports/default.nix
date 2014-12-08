/*
 * \brief  External source code for libports
 * \author Emery Hemingway
 * \date   2014-09-19
 */

{ tool ? import ../../../tool { } }:

let
  callPort = path:
    let f = (import path);
    in f (builtins.intersectAttrs (builtins.functionArgs f) (tool.nixpkgs // tool) );
in
{
  libc    = callPort ./libc.nix;
  libpng  = callPort ./libpng.nix;
  sdl     = callPort ./sdl.nix;
  sdl_net = callPort ./sdl_net.nix;
  stdcxx  = callPort ./stdcxx.nix;
  x86emu  = callPort ./x86emu.nix;
  zlib    = callPort ./zlib.nix;
}
