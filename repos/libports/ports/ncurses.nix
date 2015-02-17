{ preparePort, fetchurl, gcc, mawk, ncurses }:

preparePort rec {
  name = "ncurses-5.9";

  src = fetchurl {
    url = "mirror://gnu/ncurses/${name}.tar.gz";
    sha256 = "0fsn7xis81za62afan0vvm38bvgzg5wfmv1m86flqcj0nj7jjilh";
  };

  buildInputs = [ gcc mawk ncurses ];

  outputs = [ "source" "include" ];

  subst = map (s: "-e 's/${s}/g'")
    [ "@NCURSES_MAJOR@/5"
      "@NCURSES_MINOR@/9"
      "@NCURSES_PATCH@/20110404"
      "@NCURSES_MOUSE_VERSION@/1"
      "@NCURSES_CONST@/\\/*nothing*\\/"
      "@NCURSES_INLINE@/inline"
      "@NCURSES_OPAQUE@/0"
      "@NCURSES_INTEROP_FUNCS@/0"
      "@NCURSES_SIZE_T@/short"
      "@NCURSES_TPARM_VARARGS@/1"
      "@NCURSES_CH_T@/chtype"
      "@NCURSES_LIBUTF8@/0"
      "@NCURSES_OSPEED@/short"
      "@NCURSES_WCHAR_T@/0"
      "@NCURSES_WINT_T@/0"
      "@NCURSES_SBOOL@/char"
      "@NCURSES_XNAMES@/1"
      "@HAVE_TERMIOS_H@/1"
      "@HAVE_TCGETATTR@/1"
      "@NCURSES_CCHARW_MAX@/5"
      "@NCURSES_EXT_COLORS@/0"
      "@NCURSES_EXT_FUNCS@/1"
      "@NCURSES_SP_FUNCS@/0"
      "@NCURSES_OK_WCHAR_T@/"
      "@NCURSES_WRAP_PREFIX@/_nc_"
      "@cf_cv_header_stdbool_h@/1"
      "@cf_cv_enable_opaque@/NCURSES_OPAQUE"
      "@cf_cv_enable_reentrant@/0"
      "@cf_cv_enable_lp64@/0"
      "@cf_cv_typeof_chtype@/long"
      "@cf_cv_typeof_mmask_t@/long"
      "@cf_cv_type_of_bool@/unsigned char"
      "@cf_cv_1UL@/1UL"
      "@USE_CXX_BOOL@/defined(__cplusplus)"
      "@BROKEN_LINKER@/0"
      "@NEED_WCHAR_H@/0"
      "@GENERATED_EXT_FUNCS@/generated"
      "@HAVE_TERMIO_H@/1"
      "@HAVE_VSSCANF@/1"
    ];

  localInclude = ../include/ncurses;

  installPhase =
    ''
      apply_substitutions()
      {
        eval "sed $subst $1 > $2"
      }

      AWK=mawk

      mkdir -p $source $include/ncurses

      apply_substitutions include/curses.h.in $include/ncurses/curses.h
      include/MKkey_defs.sh include/Caps >> $include/ncurses/curses.h
      cat include/curses.tail >> $include/ncurses/curses.h

      apply_substitutions include/MKterm.h.awk.in $include/ncurses/MKterm.h.awk
      apply_substitutions include/ncurses_dll.h.in $include/ncurses/ncurses_dll.h
      apply_substitutions include/termcap.h.in $include/ncurses/termcap.h
      apply_substitutions include/unctrl.h.in $include/ncurses/unctrl.h

      include/MKncurses_def.sh include/ncurses_defs > $include/ncurses/ncurses_def.h

      include/MKparametrized.sh include/Caps > $include/ncurses/parametrized.h

      include/MKhashsize.sh include/Caps > $include/ncurses/hashsize.h

      mawk -f ncurses/tinfo/MKnames.awk bigstrings=1 include/Caps > $source/names.c

      mawk -f ncurses/tinfo/MKcodes.awk bigstrings=1 include/Caps > $source/codes.c

      cc -o make_keys -DHAVE_CONFIG_H -I$include/ncurses -I$localInclude \
                      -Iinclude -Incurses -I$source ncurses/tinfo/make_keys.c

      cc -o make_hash -DHAVE_CONFIG_H -I$include/ncurses -I$localInclude \
                      -Iinclude -Incurses -I$source ncurses/tinfo/make_hash.c

      ncurses/tinfo/MKkeys_list.sh include/Caps | sort > $include/ncurses/keys.list

      ./make_keys $include/ncurses/keys.list > $include/ncurses/init_keytry.h

      mawk -f $include/ncurses/MKterm.h.awk include/Caps > $include/ncurses/term.h

      echo gonna run this script

      ./ncurses/tinfo/MKfallback.sh x misc/terminfo.src tic linux vt102 > $source/fallback.c
      #sh -e $< /usr/share/terminfo $(ncurses_src_dir)/misc/terminfo.src /usr/bin/tic > $@

      echo | mawk -f ncurses/base/MKunctrl.awk bigstrings=1 > $source/unctrl.c


      ./ncurses/tinfo/MKcaptab.sh \
          mawk 1 ncurses/tinfo/MKcaptab.awk \
          include/Caps > $source/comp_captab.c

      cp -r ncurses/* $source

      include=$include/ncurses
      mkdir -p $include
      cp \
        include/nc_alloc.h \
        include/nc_panel.h \
        include/nc_tparm.h \
        include/term_entry.h \
        include/tic.h \
        include/hashed_db.h \
        include/capdefaults.c \
        $include
    '';
}
