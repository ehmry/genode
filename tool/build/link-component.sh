source $genodeEnv/setup

runHook preLink

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


[ "$dynamicLinker" ] && \
	ldFlags="$ldFlags --eh-frame-hdr --dynamic-linker=$dynamicLinker"

[ "$dynDl" ] && \
	ldFlags="$ldFlags --dynamic-list=$dynDl"

[ "$ldTextAddr" ] && \
	ldFlags="$ldFlags -Ttext=$ldTextAddr"

for s in $ldScripts
do ldFlags="$ldFlags -T $s"
done

for f in $extraLdFlags $ldFlags 
do cxxLdFlags="$cxxLdFlags -Wl,$f"
done

mkdir -p $out

VERBOSE $cxx \
            $cxxLdFlags $cxxLinkFlags \
	    -Wl,--whole-archive -Wl,--start-group \
            $objects $libs_ \
	    -Wl,--end-group -Wl,--no-whole-archive \
            ${finalArchives:-$($cc $ccMarch -print-libgcc-file-name)} \
            -o $out/$name

runHook postLink
