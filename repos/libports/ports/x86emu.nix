{ tool, preparePort }:

let version = "1.12.0"; in
preparePort {
  name = "x86emu";

  archive = tool.nixpkgs.fetchurl {
    url = "http://ftp.x.org/pub/individual/xserver/xorg-server-${version}.tar.bz2";
    sha256 = "16bpy1b6wrh3bx9w385dwmlnwa3ijr18bjldn6ma95w6vv2i82i8";
  };

  tarOpt = [ "--strip-components=4" "xorg-server-${version}/hw/xfree86/x86emu" ];

  patchPhase = ''
    sed -i 's/private;/private_ptr;/g' x86emu/regs.h
  '';

  #meta = {
  #  license = "MIT";
  #};
}