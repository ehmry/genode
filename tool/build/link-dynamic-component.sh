source $genodeEnv/setup

MSG_LINK $name

objects=$(sortWords $objects)

for o in $externalObjects
do objects="$objects $o/*.o"
done

libs=$(sortDerivations $libs)
libs_=""
for l in $libs
do libs_="$libs_ $l/*.a $l/*.so"
done


for f in $extraLdFlags $ldFlags --dynamic-list="$dynDl"
do cxxLdFlags="$cxxLdFlags -Wl,$f"
done

[[ "$ldTextAddr" ]] && \
    cxxLinkFlags="$cxxLinkFlags -Wl,-Ttext=$ldTextAddr"

for s in $ldScripts
do scriptFlags="$scriptFlags -Wl,-T -Wl,$s"
done

[ -d "$dynamicLinker" ] && dynamicLinker=$dynamicLinker/*.lib.so

mkdir -p $out

VERBOSE $cxx \
            $cxxLdFlags $cxxLinkFlags \
            -Wl,--dynamic-linker=$(basename $dynamicLinker) \
            -Wl,--eh-frame-hdr \
            $scriptFlags \
	    -Wl,--whole-archive -Wl,--start-group \
            $objects $libs_ \
	    -Wl,--end-group -Wl,--no-whole-archive \
            $($cc $ccMarch -print-libgcc-file-name) \
            -o $out/$name
