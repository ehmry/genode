striphash() {
    echo $(basename $i) | cut -c34-
}

cp $boot/isolinux.bin ./
chmod +w isolinux.bin

echo "boot/isolinux/=isolinux.bin" > pathlist
echo "boot/isolinux/=$boot/isolinux.cfg" >> pathlist
echo "boot/isolinux/=$boot/chain.c32" >> pathlist

echo "boot/=$boot/bender" >> pathlist
echo "boot/=$boot/pulsar" >> pathlist

echo "boot/grub/=$boot/stage2_eltorito" >> pathlist
echo "boot/grub/=menu.lst" >> pathlist

hypervisor=$(ls $hypervisor/hypervisor*)

echo "hypervisor=$hypervisor" >> pathlist

echo "genode=$genodeImage" >> pathlist

echo "timeout 0" > menu.lst
echo "default 0" >> menu.lst
echo "" >> menu.lst
echo "title Genode on NOVA" >> menu.lst
echo " kernel /boot/bender" >> menu.lst
echo " module /hypervisor iommu serial" >> menu.lst
echo " module /genode/core" >> menu.lst

echo genodeImage is $genodeImage


for i in $genodeImage/*; do
    dest="/genode/$(basename $i)"
    if [ "$dest" != "/genode/core" ]; then
        echo " module $dest" >> menu.lst
    fi
done


cat pathlist

echo $genisoimage -f -l -R -hide-rr-moved -jcharset utf-8 \
    -b boot/isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
    -graft-points -path-list pathlist \
    -o $out

$genisoimage -f -l -R -hide-rr-moved -jcharset utf-8 \
    -b boot/isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
    -graft-points -path-list pathlist \
    -o $out
