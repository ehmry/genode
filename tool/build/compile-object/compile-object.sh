source $common/setup

# Link the file into this temporary directory
# so that gcc doesn't compile the nix store
# prefix into the object, which would register
# source code as a runtime dependency.
_source=$source
source=$(stripHash $source)
ln -s $_source ./$source

base="${source%.*}"
object="${base}.o"
base="${base//./_}"

stripHash $object

MSG_COMP $object

includes=""
sourceDir=$(dirname $_source)
[ "$sourceDir" != "$NIX_STORE" ] && includes="-I$sourceDir"
for d in $includeDirs; do
    if [ -d "$d" ]; then
        includes="$includes -I$d"
    else
        echo "$d does not exist or is not a directory,"
        echo "will not compile $source"
        exit 1
    fi
done

#for lib in $libs; do
#    d="$lib/include"
#    [ -d "$d" ] && includes="$includes -I$d"
#done

mkdir -p $out

file_opt_var=ccOpt_$base
ccFlags="$ccFlags ${!file_opt_var}"

case $source in
    *.c | *.s )
        VERBOSE $cc $ccFlags $includes -c $source -o $out/$object
        ;;

    *.cc | *.cpp )
        VERBOSE $cxx $ccFlags $cxxFlags $includes -c $source -o $out/$object
        ;;

    *.S )
        VERBOSE $cc $ccFlags -D__ASSEMBLY__ $includes -c $source -o $out/$object
        ;;
    * )
        echo Unhandled source file $source
        exit 1
        ;;
esac
