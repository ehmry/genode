source @common@/setup

dumpVars

#in=$(findSource "$sourceDirs" $source)
#if [ -z "$in" ]; then
#    echo "source file $source not found in"
#    for d in $sourceDirs; do
#        echo -e "\t$d"
#    done
#    exit 1
#fi

object=$(basename $source)
object="${object/%.c/.o}"
object="${object/%.cc/.o}"
object="${object/%.S/.o}"
object="${object/%.cpp/.o}"
object="${object/%.s/.o}"

MSG_COMP "$(echo $object | cut -c34-)"

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

case $source in
    *.c | *.s )
        VERBOSE $cc $ccFlags $flags $includes -c $source -o $out #/$object
        ;;

    *.cc | *.cpp )
        VERBOSE $cxx $cxxFlags $flags $includes -c $source -o $out #/$object
        ;;

    *.S )
        VERBOSE $cc $ccFlags $flags $includes -D__ASSEMBLY__ -c $source -o $out #/$object
        ;;
esac

dumpVars
