{ preparePort, fetchurl }:

let
  version = "1.12.0"; 
  # LICENSE := MIT
in
preparePort {
  name = "x86emu-${version}";

  src = fetchurl {
    url = "http://ftp.x.org/pub/individual/xserver/xorg-server-${version}.tar.bz2";
    sha256 = "16bpy1b6wrh3bx9w385dwmlnwa3ijr18bjldn6ma95w6vv2i82i8";
  };

  tarFlags = [ "--strip-components=4" "xorg-server-${version}/hw/xfree86/x86emu" ];

  patchPhase = ''
    sed -i 's/private;/private_ptr;/g' regs.h
  '';
}