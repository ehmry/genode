{ linkComponent, compileCC, nixpkgs
, base, config, libc, libpng, zlib, blit, file }:

linkComponent rec {
  name = "menu_view";
  libs = [ base config libc libpng zlib blit file ];
  objects = compileCC {
    src = ./main.cc;
    inherit libs;
    includes = [ ../menu_view ../../../../demo/include ];
  };

  tar = nixpkgs.gnutar;
  postLink =
    ''
      cp -r ${./styles} styles
      ${tar}/bin/tar cf $out/menu_view_styles.tar styles
    '';
}