source $genodeEnv/setup

for s in $*
do source $s
done

# turn into an arrays
sources=($sources)
includes=($includes)

runHook gatherPhase

findLibs() {
    local pkg=$1
    local var=$2

    lib=$(echo $pkg/*.a; echo $pkg/*.so)
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

dumpVars
for ((n = 1; n < ${#sources[*]}; n += 2)); do
    compile ${sources[$n]}
done

#runHook preMerge
runHook mergePhase
#runHook postMerge

dumpVars
