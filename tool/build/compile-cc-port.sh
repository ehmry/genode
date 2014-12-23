source $genodeEnv/setup

[ "$sourceRoot" ] && pushd $sourceRoot

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

    base="${base%.cc}"
    base="${base%.cpp}"
    object="${base}.o"

    # apply per-file ccFlags
    var="ccFlags_${base//./_}"

    VERBOSE $cxx ${!var} $extraFlags $ccFlags $cxxFlags $includeFlags \
            -c "$src" -o "$out/$object"
done
