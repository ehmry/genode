{ stdenv, nixpkgs, dhallApps, NOVA, base-nova }:

nixpkgs.writeScriptBin "nova-iso" (with nixpkgs.buildPackages;
  let inherit (stdenv) cc;

  in ''
    #!${runtimeShell}
    set -eu

    CC="${cc}/bin/${cc.targetPrefix}cc"
    LD="${buildPackages.binutils}/bin/${buildPackages.binutils.targetPrefix}ld"
    SYSLINUX="${syslinux}/share/syslinux"

    TMPDIR="$(${coreutils}/bin/mktemp -p /tmp -d  nova-iso.XXXX)"
    mkdir -p "$TMPDIR/boot/syslinux"
    trap "rm -rf $TMPDIR" err exit

    CORE_NOVA="${base-nova}/lib/core-nova.o"

    ${dhallApps.dhall.program} text <<< "(${
      ./modules.as.dhall
    }) ($@)" > "$TMPDIR/modules.as"

    # compile the boot modules into one object file
    $CC -c -x assembler -o "$TMPDIR/boot_modules.o" "$TMPDIR/modules.as"

    # link final image
    $LD -nostdlib \
    	-T${../../repos/base/src/ld/genode.ld} \
    	-T${../../repos/base-nova/src/core/core-bss.ld} \
    	-z max-page-size=0x1000 \
    	-Ttext=0x100000 -gc-sections \
    	"$CORE_NOVA" "$TMPDIR/boot_modules.o" \
    	-o "$TMPDIR/boot/image.elf"

    pushd "$TMPDIR"

    strip boot/image.elf

    # build ISO image
    cp ${NOVA}/hypervisor* boot/hypervisor
    cp ${./isolinux.cfg} boot/syslinux/isolinux.cfg
    cp \
    	$SYSLINUX/isolinux.bin \
    	$SYSLINUX/ldlinux.c32 \
    	$SYSLINUX/libcom32.c32 \
    	$SYSLINUX/mboot.c32 \
    	boot/syslinux
    chmod +w boot/syslinux/isolinux.bin

    ISO_FILE="''${DIRSTACK[1]}/nova.iso"

    mkisofs -o "$ISO_FILE" \
    	-b syslinux/isolinux.bin -c syslinux/boot.cat \
    	-no-emul-boot -boot-load-size 4 -boot-info-table \
    	-iso-level 2 \
    	boot

    popd

    # build test script
    QEMU_SCRIPT=boot-qemu.sh
    cat > "$QEMU_SCRIPT" << EOF
    #!/bin/sh
    qemu-system-x86_64 -cdrom nova.iso -machine q35 -serial mon:stdio \$@
    EOF

    chmod +x "$QEMU_SCRIPT"
  '')
