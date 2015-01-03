mkdir -p genode isolinux

# Create the image filetree.
sources_=($sources)
targets_=($targets)
for ((i = 0; i < ${#targets_[@]}; i++)); do
    target="${targets_[$i]}"
    source="${sources_[$i]}"

    if [ -d "$source" ]; then
        mkdir -p genode/$target
        ln -sf ${source}/* "genode/$target/"
    else
        mkdir -p genode/$(dirname $target)
        ln -sf "${source}" "genode/$target"
    fi
done

kernel_=$kernel
kernel=$(basename $kernel_)
objcopy -O elf32-i386 ${kernel_} $kernel

cp --no-preserve mode \
   $syslinux/share/syslinux/isolinux.bin \
   $syslinux/share/syslinux/ldlinux.c32 \
   $syslinux/share/syslinux/mboot.c32 \
   $syslinux/share/syslinux/libcom32.c32 \
   isolinux

chmod a+rX -R ./

# Write isolinux config.
cat > isolinux/isolinux.cfg <<EOF
SERIAL 0
DEFAULT genode

LABEL genode
  KERNEL mboot.c32
  APPEND /$kernel $kernelArgs --- /genode/core\
$(for i in genode/*
    do [ "$i" != "genode/core" ] && echo -n " --- /$i"
done)
EOF

genisoimage -o $out \
    -f -l -R -hide-rr-moved -jcharset utf-8 \
    -b isolinux/isolinux.bin -c isolinux/boot.cat \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    ./
