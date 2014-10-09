#
# \brief  Shell script to compile source code
# \author Emery Hemingway
# \date   2014-10-21
#

compile() {
    main=$1

    echo -e "    COMPILE  $@"

    base="${main%.*}"
    object="${base}.o"
    base="${base//./_}"
    
    file_opt_var=ccOpt_$base
    ccFlags="$ccFlags ${!file_opt_var}"

    includeOpts="-I."
    for i in $includePaths $nativeIncludePaths
    do includeOpts="$includeOpts -I$i"
    done

    test "$prefix" && cd $prefix

    case $main in
        *.c | *.s )
            VERBOSE $cc $ccFlags $includeOpts \
                -c $main -o $object
            ;;

        *.cc | *.cpp )
            VERBOSE $cxx $ccFlags $cxxFlags \
                $includeOpts -c $main -o $object
            ;;

        *.S )
            VERBOSE $cc $ccFlags -D__ASSEMBLY__ \
                $includeOpts -c $main -o $object
            ;;
        * )
            echo Unhandled source file $main
            exit 1
            ;;
    esac
    
    objects="$objects $object"
}
