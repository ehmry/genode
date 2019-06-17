{ pkgs ? import <nixpkgs> {} }: with pkgs;

stdenv.mkDerivation {
  name = "genode-dev-env";
  buildInputs =
    [ (import ./toolchain.nix { inherit pkgs; })
      git
      ccache
      tcl
      gnumake
      which
      findutils
      autoconf264
      autogen

      expect libxml2 syslinux qemu xorriso

      # libc
      flex bison

      # virtualbox
      yasm libxslt iasl

      # qt5
      gperf
      qt5.full

      # ncurses
      mawk

      # fb_sdl
      pkgconfig
      SDL.dev
      alsaLib.dev


      # ncurses port needs infocmp
      ncurses
    ];

  shellHook =
    ''
      export PROMPT_DIRTRIM=2
      export PS1="\[\033[1;30m\]Genode-dev [\[\033[1;37m\]\w\[\033[1;30m\]] $\[\033[0m\] "
      export PS2="\[\033[1;30m\]>\[\033[0m\] "
    '';
}
