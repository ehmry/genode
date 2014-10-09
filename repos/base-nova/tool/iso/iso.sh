cd $image_dir

for c in genode/*; do
    if [ ! "$c" == "genode/core" ]; then
        grubCfg="$grubCfg  module /$c\n"
    fi
done

hypervisor=$(basename $image_dir/boot/hypervisor?*)

cd -
echo -e "$grubCfg}" > grub.cfg

grub-mkrescue -o $out -- \
    -follow link \
    -map $image_dir / \
    -map $bender /boot/bender \
    -map grub.cfg /boot/grub/grub.cfg \
    -lns $hypervisor /boot/hypervisor
