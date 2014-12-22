{ preparePort, fetchurl, gcc }:

preparePort rec {
  version = "4.2";
  name = "oss-v4.2-build2006-src-bsd";

  src = fetchurl {
    url = "http://www.4front-tech.com/developer/sources/stable/bsd/${name}.tar.bz2";
    sha256 = "1pnfzfa6s8a44pdqcznj8hpawwgn5ji64wb2nvk4n251h2mwkgi1";
  };

  patches = "${../patches}/*.patch";

  buildInputs = [ gcc ];
  
  installPhase =
    ''
      copySource() {
        dir=$1
	shift
        mkdir -p $source/$dir
	cd $dir
	cp -r $* $source/$dir
	cd -
      }
    
      # needed for preparation
      copySource setup \
          srcconf.c srcconf_freebsd.inc \
	  srcconf_vxworks.inc gen_driver_freebsd.inc

      # oss framework
      copySource kernel/framework/include \
          oss_config.h oss_memblk.h \
          oss_version.h audio_core.h mixer_core.h oss_calls.h \
          internal.h oss_pci.h spdif.h midi_core.h grc3.h ac97.h \

      copySource kernel/framework/include/ossddk \
          oss_exports.h oss_limits.PHh ossddk.h

      # oss core
      copySource kernel/framework/osscore \
          oss_memblk.c oss_core_options.c oss_core_services.c

      copySource include soundcard.h

      # audio core
      copySource kernel/framework/audio \
          oss_audio_core.c oss_spdif.c oss_audiofmt.c \
          ulaw.h audiocnv.inc oss_grc3.c fltdata2_h.inc \
          grc3code.inc grc3inc.inc

      # mixer core
      copySource kernel/framework/mixer \
          oss_mixer_core.c mixerdefs.h

      # vmixer core
      copySource kernel/framework/vmix_core \
          vmix_core.c vmix_input.c vmix.h db_scale.h \
          vmix_import.inc  vmix_import_int.inc \
          rec_export.inc rec_export_int.inc \
          vmix_output.c outexport.inc outexport_int.inc \
          playmix.inc playmix_int.inc playmix_src.inc

      # midi core
      copySource kernel/framework/midi \
          oss_midi_core.c oss_midi_timers.c oss_midi_mapper.c \
          oss_midi_queue.c

      # ac97 core
      copySource kernel/framework/ac97 '*'

      # drivers
      copySource kernel/drv \
          oss_ich oss_hdaudio oss_audiopci

      # Build and execute 'srcconf' utility, build 'devices.list'
      ln -sf $source/setup/srcconf_freebsd.inc $source/setup/srcconf_linux.inc
      ln -sf $source/setup/gen_driver_freebsd.inc $source/setup/gen_driver_linux.inc
      mkdir -p $source/kernel/framework/include
      mkdir -p $source/kernel/OS/Linux
      echo cc -g -I$source/setup $includeFlags -o srcconf $source/setup/srcconf.c
      cc -g -I$source/setup -o srcconf $source/setup/srcconf.c
      rm -f $source/kernel/framework/include/ossddk/oss_limits.h
      cat `find $source/kernel/drv -name .devices`| grep -v '^#' > $source/devices.list
      cp ./srcconf $source; cd $source; ./srcconf; rm srcconf
      cd $source/target/build ; for f in *.c; do mv $f pci_$f; done
    '';
  
}
