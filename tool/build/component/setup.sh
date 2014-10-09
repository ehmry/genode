source @common@/setup

_objects=$objects
objects=""
    
for o in $_objects; do
    objects="$objects $o/*.o"
done

findLibs() {
    local pkg=$1
    local var=$2


    lib=$(echo $i/*.a; echo $i/*.so)
    case ${!var} in
        *\ $lib\ *)
            return 0
            ;;
    esac

    eval $var="'${!var} $lib '"

    if [ -f $pkg/nix-support/propagated-libraries ]; then
        for i in $(cat $pkg/nix-support/propagated-libraries); do
            findLibs $i $var
        done
    fi
}

_libs=$libs
libs=""
for i in $_libs; do
    findLibs $i libs
done


linkPhase() {
    runHook preLink
    MSG_LINK $name

    mkdir -p $out

    VERBOSE $cxx $cxxLinkOpt \
	-Wl,--whole-archive -Wl,--start-group \
        $objects $libs \
	-Wl,--end-group -Wl,--no-whole-archive \
        $($cc $ccMarch -print-libgcc-file-name) \
        -o $out/$name

    runHook postLink
}


fixupPhase() {
    mkdir $src

    for s in $objectSources; do
        cp -rLf --no-preserve=mode $s/* $src
    done

    dumpVars
    cp env-vars $src
}

componentBuild() {
    MSG_PRG $name
    genericBuild
}

