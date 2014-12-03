source $genodeEnv/setup

MSG_MERGE $name

objects=$(sortDerivations $objects)

for o in $externalObjects
do objects="$objects $o/*.o"
done

libs=$(sortDerivations $libs)
libs_=""
for l in $libs
do libs_="$libs_ $l/*.a $l/*.so"
done

mkdir -p $out

VERBOSE $ld -o $out/$name.lib.so \
            -shared --eh-frame-hdr \
            $extraLdFlags $ldFlags \
	    -T $ldScriptSo \
            --entry=$entryPoint \
	    --whole-archive \
	    --start-group \
            $libs_ $objects \
	    --end-group \
	    --no-whole-archive \
            $($cc $ccMarch -print-libgcc-file-name)
