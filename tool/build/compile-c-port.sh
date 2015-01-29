# TODO: revise and cleanup
# This script was converted from a postCompile hook
# and retains some hook artifacts.

source $genodeEnv/setup

if [ "$sourceRoot" ]; then
    echo sourceRoot is $sourceRoot
    cd $sourceRoot
fi

for i in $localIncludes $systemIncludes
do includeFlags="$includeFlags -I $i"
done
mkdir -p $out

[ "$PIC" ] && ccFlags="$ccFlags -fPIC"

for src in $sources; do
    base=$(basename "$src")
    case $filter in
        *$base*)
            continue;;
    esac

    MSG_COMP $base

    base="${base%.?}"
    object="${base}.o"

    # apply per-file ccFlags
    var="ccFlags_${base//./_}"

    VERBOSE $cc ${!var} $extraFlags $ccFlags $optimization $includeFlags \
            -c "$src" -o "$out/$object"
done
