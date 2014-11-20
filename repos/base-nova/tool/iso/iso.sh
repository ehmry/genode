source $genodeEnv/setup

cp -rL --no-preserve mode $image_dir/* ./

rm -fr genode/nix-support

for c in genode/*
do [ ! "$c" == "genode/core" ] && syslinuxCfg="$syslinuxCfg --- /$c"
done

mkdir -p isolinux

echo -e "$syslinuxCfg" > isolinux/isolinux.cfg

cp --no-preserve mode \
   $syslinux/share/syslinux/isolinux.bin \
   $syslinux/share/syslinux/ldlinux.c32 \
   $syslinux/share/syslinux/mboot.c32 \
   $syslinux/share/syslinux/libcom32.c32 \
   isolinux

chmod a+rX -R ./

$objcopy -O elf32-i386 hypervisor hypervisor

$cdrkit/bin/genisoimage -o $out \
    -f -l -R -hide-rr-moved -jcharset utf-8 \
    -b isolinux/isolinux.bin -c isolinux/boot.cat \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    ./
