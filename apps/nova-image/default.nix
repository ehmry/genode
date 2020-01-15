{ stdenv, nixpkgs, dhallApps, NOVA, base-nova }:

nixpkgs.writeScriptBin "nova-image" (with nixpkgs.buildPackages;
  let inherit (stdenv) cc;

  in ''
    #!${runtimeShell}
    set -eu

    CC="${cc}/bin/${cc.targetPrefix}cc"
    LD="${buildPackages.binutils}/bin/${buildPackages.binutils.targetPrefix}ld"

    TMPDIR="$(${coreutils}/bin/mktemp -p /tmp -d  nova-iso.XXXX)"
    # trap "rm -rf $TMPDIR" err exit

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
    	-o "image.elf"
  '')
