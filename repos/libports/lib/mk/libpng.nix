/*
 * \author Emery Hemingway
 * \date   2014-09-30
 */

{ tool }: with tool;
{ libpng }:
{ libc, libm, zlib }:

buildLibrary {
  name = "libpng";
  shared = true;
  libs = [ libc libm zlib ];

  ccDef = [ "-DHAVE_CONFIG_H" "-DPNG_CONFIGURE_LIBPNG" ];

  sources = fromDir libpng [
    "png.c" "pngset.c" "pngget.c" "pngrutil.c" "pngtrans.c"
    "pngwutil.c" "pngread.c" "pngrio.c" "pngwio.c" 
    "pngwrite.c" "pngrtran.c" "pngwtran.c" "pngmem.c" 
    "pngerror.c" "pngpread.c"
  ];
  includeDirs = [ ../../src/lib/libpng ]; # find 'config.h'
  propagatedIncludes = 
    [ (newDir "libpng-include" (
        fromDir libpng [ "pngconf.h" "png.h" "pngpriv.h" ]))
    ];
}