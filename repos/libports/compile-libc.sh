# TODO: revise and cleanup
# This script was converted from a postCompile hook
# and retains some hook artifacts.

source $genodeEnv/setup

[ "$sourceRoot" ] && pushd $sourceRoot

findIncludes() {
    findLocalIncludes  $1
    findSystemIncludes $1
}

findLocalInclude() {
    local file=$1

    if [ ! -e "$file" ]; then
        for dir in $NIX_BUILD_TOP $(dirname $file) $localIncludes; do
            if [ -e "$dir/$file" ]; then
                echo "$dir/$file"
                return 0
            fi
        done
        if [ ! -e "$file" ]; then
            return 1
        fi
    fi
}

findSystemInclude() {
    local file=$1

    if [ ! -e "$file" ]; then
        for dir in $NIX_BUILD_TOP/include $systemIncludes; do
            if [ -e "$dir/$file" ]; then
                echo "$dir/$file"
                return 0
            fi
        done
        if [ ! -e "$file" ]; then
            return 1
        fi
    fi
}

findLocalIncludes() {
    local found=$(grep -Pho '(?<=#include ").*(?=")' $1 || return 0)
    for inc in $found; do
        case $incLoc in
            *\ $inc\ *)
                # redundant
                ;;

            *)
                incLoc="$incLoc $inc"
                local incFile=$(findLocalInclude $inc) || continue
                findIncludes $incFile
                ;;
        esac
    done
}

findSystemIncludes() {
    local found=$(grep -Pho '(?<=#include <).*(?=>)' $1 || return 0)
    for inc in $found; do
        case $incSys in
            *\ $inc\ *)
                # redundant
                ;;

            *)
                incSys="$incSys $inc"
                local incFile=$(findSystemInclude $inc) || continue
                findIncludes $incFile
                ;;
        esac
    done
}

checkCollision() {
    [ "$(readlink -f $1)" == "$(readlink -f $2)" ] && return 0
    cmp $1 $2 && return 0

    echo "file collision: $(readlink -f $1), $(readlink -f $2)"
    return 1
}

for source in $sources; do
    bn=$(basename $source)
    case $filter in
        *$bn*)
            continue;;
    esac

    findIncludes $source

    dir=$NIX_BUILD_TOP/$(dirname $source)

    # Link the source into the temporary directory.
    mkdir -p $dir
    ln -sf $(readlink -f $source) $dir/

    # Link the local includes relative to the source file.
    for rel in $incLoc; do
        target=$dir/$rel
        if [ -e "$target" ]; then
            #checkCollision $target $abs
            continue
        fi

        abs=
        for incDir in $NIX_BUILD_TOP $(dirname $source) $localIncludes; do
            if [ -e "$incDir/$rel" ]; then
                abs="$incDir/$rel"
                break
            fi
        done

        if [ -z "$abs" ]; then
            # Its okay if we don't find them all,
            # many may be surrounded by an 'ifdef'.
            echo Included file \"$rel\" not found.
            continue
        fi

        mkdir -p $(dirname $target)
        ln -s $(readlink -f $abs) $target
    done

    # Link the system includes into a temporary 'include' directory.
    for rel in $incSys; do
        target=$NIX_BUILD_TOP/include/$rel
        if [ -e "$target" ]; then
            #checkCollision $target $abs
            continue
        fi

        abs=
        for incDir in $systemIncludes; do
            if [ -e "$incDir/$rel" ]; then
                abs="$incDir/$rel"
                break
            fi
        done

        if [ -z "$abs" ]; then
            # Its okay if we don't find them all,
            # many may be surrounded by an 'ifdef'.
            echo Included file \"$rel\" not found.
            continue
        fi

        mkdir -p $(dirname $target)
        ln -s $(readlink -f $abs) $target
    done

done

popd

includeOpts="$includeOpts -I include"

mkdir -p $out

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

    VERBOSE $cc ${!var} $extraFlags $ccFlags $includeOpts \
                -c "$src" -o "$out/$object"
done
