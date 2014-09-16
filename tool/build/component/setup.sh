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

libs=""
for i in $searchLibs; do
    findLibs $i libs
done


linkPhase() {
    runHook preLink
    MSG_LINK $name

    extraFlags=""
    if [ -n "$ldTextAddr" ]; then
        extraFlags="-Wl,-Ttext=$ldTextAddr"
    fi

    if [ -n "$ldScriptStatic" ]; then
        for s in $ldScriptStatic; do
            extraFlags="$extraFlags -Wl,-T -Wl,$s"
        done
    fi

    mkdir -p $out

    VERBOSE $cxx $cxxLinkOpt $extraFlags $ccMarch \
	-Wl,--whole-archive -Wl,--start-group \
        $objects $libs \
	-Wl,--end-group -Wl,--no-whole-archive \
        $($cc $ccMarch -print-libgcc-file-name) \
        -o $out/$name

    runHook postLink
}

componentBuild() {
    MSG_PRG $name
    genericBuild
}

dumpVars
