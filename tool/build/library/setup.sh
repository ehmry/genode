source @common@/setup

mergeStaticPhase() {
    MSG_MERGE $name

    mkdir -p $out

    VERBOSE $ar -rc $out/$name.lib.a $objects
}

mergeSharedPhase() {
    MSG_MERGE $name

    mkdir -p $out

    VERBOSE $ld -o $out/$name.lib.so -shared --eh-frame-hdr \
        $ldOpt \
	-T $ldScriptSo \
        --entry=$entryPoint \
	--whole-archive \
	--start-group \
	$usedSoFiles $staticLibsBrief $objects $libs\
	--end-group \
	--no-whole-archive \
        $($cc $ccMarch -print-libgcc-file-name)
}

fixupPhase() {
    if [ -n "libs" ]; then
        mkdir -p "$out/nix-support"
        echo "$libs" > "$out/nix-support/propagated-libraries"
    fi
}

libraryBuild() {
    MSG_LIB $name
    genericBuild
}

dumpVars
