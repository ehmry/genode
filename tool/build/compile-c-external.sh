source $genodeEnv/setup

# Turn these into an arrays.
relative=($relative)
absolute=($absolute)

# Determine how many `..' levels appear in the header file references.
# E.g., if there is some reference `../../foo.h', then we have to
# insert two extra levels in the directory structure, so that `a.c' is
# stored at `dotdot/dotdot/a.c', and a reference from it to
# `../../foo.h' resolves to `dotdot/dotdot/../../foo.h' == `foo.h'.
n=0
maxDepth=0
for ((n = 0; n < ${#relative[*]}; n += 1)); do
    target=${relative[n]}

    # Split the target name into path components using some IFS magic.
    savedIFS="$IFS"
    IFS=/
    components=($target)
    depth=0
    for ((m = 0; m < ${#components[*]}; m++)); do
        c=${components[m]}
        if test "$c" = ".."; then
            depth=$((depth + 1))
        fi
    done
    IFS="$savedIFS"

    if test $depth -gt $maxDepth; then
        maxDepth=$depth;
    fi
done

# Create the extra levels in the directory hierarchy.
prefix=
for ((n = 0; n < maxDepth; n++)); do
    prefix="dotdot/$prefix"
done

# Create symlinks to the header files.
for ((n = 0; n < ${#relative[*]}; n += 1)); do
    source=${absolute[n]}
    target=${relative[n]}

    # Create missing directories.  We use IFS magic to split the path
    # into path components.
    savedIFS="$IFS"
    IFS=/
    components=($prefix$target)
    fullPath=(.)
    for ((m = 0; m < ${#components[*]} - 1; m++)); do
        fullPath=("${fullPath[@]}" ${components[m]})
        if ! test -d "${fullPath[*]}"; then
            mkdir "${fullPath[*]}"
        fi
    done
    IFS="$savedIFS"

    ln -sf $source $prefix$target
done

test "$prefix" && cd $prefix

[ "$PIC" ] && ccFlags="$ccFlags -fPIC"

for i in . $externalIncludes
do includeFlags="$includeFlags -I $i"
done

mkdir -p $out

for src in $sources; do
    srcName=$(basename "$src")
    case $filter in
    *$srcName*)
        continue;;
    esac

    # Create a symlink to the source file.
    if ! test "$(readlink $prefix$srcName)" = $src; then
        ln -s $src $prefix$srcName
    fi

    base="${srcName%.?}"
    object="${base}.o"

    MSG_COMP $base

    # apply per-file ccFlags
    var="ccFlags_${base//./_}"

    VERBOSE $cc ${!var} $extraFlags $ccFlags $optimization $includeFlags \
                -c "$src" -o "$out/$object"
done
