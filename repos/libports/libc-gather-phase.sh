[ "$sourceRoot" ] && cd $sourceRoot

fiN=0

incDir="$incDir $incDirSh"

findIncludes() {
    ((fiN += 1))
    file=$1
    var=$2
    from=$3

    if [ ! -e "$file" ]; then
        for dir in $NIX_BUILD_TOP $incDir; do
            if [ -e "$dir/$file" ]; then
                file="$dir/$file"
                break
            fi
        done
        if [ ! -e "$file" ]; then
            echo "$file included from $from from not found"
            return 0
        fi
    fi

    found=$(grep -Pho '(?<=#include [<|"]).*(?="|>)' $file || return 0)
    for f in $found; do
        case ${!var} in
            *\ $f\ *)
                # redundant
                ;;

            *)
                eval $var="'${!var} $f '"
                findIncludes $f $var $file
                ;;
        esac
    done
}

# Prune, parse, and copy the source files.
for i in $srcSh; do
    case $filter in
        *$i*)
            continue;;
    esac

    # Recursively find relative include paths.
    findIncludes $i relIncludes $i

    bn=$(basename $i)
    if [ -e "$NIX_BUILD_TOP/$prefix$bn" ]; then
        echo "$prefix$bn alrready exists"
        exit 1
    fi
    cp $i "$NIX_BUILD_TOP/$prefix$bn"
    # setup.sh only compiles every other array element,
    # not very elegant is it.
    sources="$sources $bn $bn "
done

for i in $src; do
    case $filter in
        *$i*)
            continue;;
    esac

    # Recursively find relative include paths.
    findIncludes $i relIncludes $i

    bn=$(basename $i)
    cp $i "$NIX_BUILD_TOP/$prefix$bn"
    # setup.sh only compiles every other array element,
    # not very elegant is it.
    sources="$sources $bn $bn "
done

echo findIncludes ran $fiN times and found $(echo $relIncludes|wc -w) files

# Convert to an array.
sources=($sources)

# Construct a parallel array of absolute
# Determine how many `..' levels appear in the header file 
# references. E.g., if there is some reference `../../foo.h', 
# then we have to insert two extra levels in the directory 
# structure, so that `a.c' is stored at `dotdot/dotdot/a.c',
# and a reference from it to `../../foo.h' resolves to 
# `dotdot/dotdot/../../foo.h' == `foo.h'.
maxDepth=0
for ((n = 0; n < ${#relIncludes[*]}; n += 2)); do
    target=${relIncludes[$((n + 1))]}
    # Split the target name into path components using some
    # IFS magic.
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

# Copy the header files.
for target in $relIncludes; do
    [ -e "$NIX_BUILD_TOP/$target" ] && continue

    source=
    for dir in $incDir; do
        if [ -e "$dir/$target" ]; then
            source="$dir/$target"
            break
        fi
    done

    if [ -z "$source" ]; then
        echo "$target not found in any of the following: $incDir"
        continue
    fi

    # Create missing directories.  We use IFS magic to split the path
    # into path components.
    savedIFS="$IFS"
    IFS=/
    components=($prefix$target)
    fullPath=(.)
    for ((m = 0; m < ${#components[*]} - 1; m++)); do
        fullPath=("${fullPath[@]}" ${components[m]})
        if ! test -d "$NIX_BUILD_TOP/${fullPath[*]}"; then
            mkdir -p "$NIX_BUILD_TOP/${fullPath[*]}"
        fi
    done
    IFS="$savedIFS"

    target=$NIX_BUILD_TOP/$prefix$target

    if [ -e "$target" ]; then
        if [ "$(readlink $target)" != "$source" ]; then
            echo "multiple versions of $target found!"
            echo -e \t$(readlink $target)
            echo -e \t$source
            exit 1
        fi
    else
        cp $source $target
    fi
done

cd $NIX_BUILD_TOP
