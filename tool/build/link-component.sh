#
# \brief  Shell script for linking components
# \author Emery Hemingway
# \date   2014-10-21
#

mergePhase() {
    echo -e "    LINK     $name"

    local _libs=$libs
    local libs=""

    for l in $_libs
    do findLibs $l libs
    done

    mkdir -p $out

    VERBOSE $cxx $cxxLinkOpt \
	-Wl,--whole-archive -Wl,--start-group \
        $objects $libs \
	-Wl,--end-group -Wl,--no-whole-archive \
        $($cc $ccMarch -print-libgcc-file-name) \
        -o $out/$name
}
