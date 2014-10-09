# based on Eelco Dostra's nix-make
source $common/setup

mainName=$(stripHash $main)

MSG_COMP $mainName

# Turn $includes into an array.
localIncludes=($localIncludes)

#for ((n = 0; n < ${#localIncludes[*]}; n += 2)); do
#    echo "${localIncludes[$((n + 0))]} <- ${localIncludes[$((n + 1))]}#"
#done

# Determine how many `..' levels appear in the header file references.
# E.g., if there is some reference `../../foo.h', then we have to
# insert two extra levels in the directory structure, so that `a.c' is
# stored at `dotdot/dotdot/a.c', and a reference from it to
# `../../foo.h' resolves to `dotdot/dotdot/../../foo.h' == `foo.h'.
n=0
maxDepth=0
for ((n = 0; n < ${#localIncludes[*]}; n += 2)); do
    target=${localIncludes[$((n + 1))]}

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
for ((n = 0; n < ${#localIncludes[*]}; n += 2)); do
    source=${localIncludes[n]}
    target=${localIncludes[$((n + 1))]}

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

# Create a symlink to the main file.
if ! test "$(readlink $prefix$mainName)" = $main; then
    ln -s $main $prefix$mainName
fi

base="${mainName%.*}"
object="${base}.o"
base="${base//./_}"

file_opt_var=ccOpt_$base
ccFlags="$ccFlags ${!file_opt_var}"

includeOpts="-I."
for i in $nativeIncludePaths
do includeOpts="$includeOpts -I$i"
done

mkdir $out $src

test "$prefix" && cd $prefix

case $mainName in
    *.c | *.s )
        VERBOSE $cc $ccFlags $includeOpts -c $mainName -o $out/$object
        ;;

    *.cc | *.cpp )
        VERBOSE $cxx $ccFlags $cxxFlags $includeOpts -c $mainName -o $out/$object
        ;;

    *.S )
        VERBOSE $cc $ccFlags -D__ASSEMBLY__ $includeOpts -c $mainName -o $out/$object
        ;;
    * )
        echo Unhandled source file $mainName
        exit 1
        ;;
esac

cp -r ./* $src
