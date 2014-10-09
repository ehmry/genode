source @common@/setup

_objects=$objects
objects=""

for o in $_objects
do objects="$objects $o/*.o"
done


mergeStaticPhase() {
    MSG_MERGE $name

    mkdir -p $out

    VERBOSE $ar -rc $out/$name.lib.a $objects
}


mergeSharedPhase() {
    local _libs=$libs
    local libs=""

    for l in $_libs
    do libs="$libs $l/*.a $l/*.so"
    done

    MSG_MERGE $name

    mkdir -p $out

    VERBOSE $ld -o $out/$name.lib.so -shared --eh-frame-hdr \
        $ldFlags \
	-T $ldScriptSo \
        --entry=$entryPoint \
	--whole-archive \
	--start-group \
        $libs $objects \
	--end-group \
	--no-whole-archive \
        $($cc $ccMarch -print-libgcc-file-name)
}

fixupPhase() {
    mkdir $src

    for s in $objectSources; do
        cp -rL --no-preserve=mode $s/* $src
    done

    dumpVars
    cp env-vars $src

    if [ -z "$shared" ] && [ -n "$libs" ]; then
        mkdir -p "$out/nix-support"
        echo "$libs" > "$out/nix-support/propagated-libraries"
    fi
}

libraryBuild() {
    MSG_LIB $name
    genericBuild
}
