source @common@/setup

objects=""

includeOpts=""
for i in $includeDirs; do
    includeOpts="$includeOpts -I$i"
done

compile() {
    MSG_COMP $mainName

    local file_opt_var=ccOpt_$base
    local ccFlags="$ccFlags ${!file_opt_var}"

    case $mainName in
        *.c | *.s )
            VERBOSE $cc $ccFlags $includeOpts -c $main \
                -o $NIX_BUILD_TOP/$object
            ;;
        *.cc | *.cpp )
            VERBOSE $cxx $ccFlags $cxxFlags $includeOpts -c $main \
                -o $NIX_BUILD_TOP/$object
            ;;
        *.S )
            VERBOSE $cc $ccFlags -D__ASSEMBLY__ $includeOpts -c $main \
                -o $NIX_BUILD_TOP/$object
            ;;        
        * )
            echo Unhandled source file $main
            exit 1
            ;;
    esac

    echo $object
}

compilePhase() {
    cd $src
    paths=$(ls $sources)
    if [ -z "$paths" ]; then
	echo "$sources not found"
	exit 1
    fi
    
    for main in $paths; do
        mainName=$(basename "$main")

        # test if $main is in filters
        filtered=""
        for f in $filters; do
            if [ "$mainName" == "$f" ]; then
                filtered=1
                break
            fi
        done
        [ "$filtered" ] && continue

        base="${mainName%.*}"
        object="${base}.o"
        base="${base//./_}"

        compile $main $base $object

        objects="$objects $object"
    done

    cd -
}

mergeStaticPhase() {
    MSG_MERGE $name
    mkdir $out
    VERBOSE $ar -rc $out/$name.lib.a $objects
}


mergeSharedPhase() {
    local _libs=$libs
    local libs=""

    for l in $_libs
    do libs="$libs $l/*.a $l/*.so"
    done

    MSG_MERGE $name

    mkdir $out

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
    if [ -z "$shared" ] && [ -n "$libs" ]; then
        mkdir -p "$out/nix-support"
        echo "$libs" > "$out/nix-support/propagated-libraries"
    fi
}

libraryBuild() {
    MSG_LIB $name
    genericBuild
}
