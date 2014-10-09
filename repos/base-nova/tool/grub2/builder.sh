mkdir -p $out/boot/grub $out/genode
cp $hypervisor/* /boot/hypervisor
cp $core/core $init/init $out/genode
