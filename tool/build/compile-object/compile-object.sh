source $common/setup

dumpVars

object=$(basename "$source")

if echo "$object" | grep -q '^[a-z0-9]\{32\}-'; then
    object=$(echo "$object" | cut -c34-)
fi

object="${object/%.c/.o}"
object="${object/%.cc/.o}"
object="${object/%.S/.o}"
object="${object/%.cpp/.o}"
object="${object/%.s/.o}"

stripHash $object

MSG_COMP $object

includes=""
for d in $includeDirs; do
    if [ -d "$d" ]; then
        includes="$includes -I$d"
    else
        echo "$d does not exist or is not a directory,"
        echo "will not compile $source"
        exit 1
    fi
done

mkdir -p $out

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

dumpVars
