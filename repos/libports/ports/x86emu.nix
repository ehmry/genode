{ preparePort, fetchurl }:

let
  version = "1.12.0";
in
preparePort {
  name = "x86emu-${version}";
  outputs = [ "source" "include" ];

  src = fetchurl {
    url = "http://ftp.x.org/pub/individual/xserver/xorg-server-${version}.tar.bz2";
    sha256 = "16bpy1b6wrh3bx9w385dwmlnwa3ijr18bjldn6ma95w6vv2i82i8";
  };

  tarFlags = [ "--strip-components=4" "xorg-server-${version}/hw/xfree86/x86emu" ];
  sourceRoot = ".";

  patchPhase =
    ''
      sed -i 's/private;/private_ptr;/g' x86emu/regs.h
    '';

  preInstall =
    ''
      mkdir -p $include/x86emu/x86emu
      cp x86emu.h $include/x86emu/
      cp x86emu/regs.h x86emu/types.h $include/x86emu/x86emu/
    '';

  #meta.license = "MIT";
}
