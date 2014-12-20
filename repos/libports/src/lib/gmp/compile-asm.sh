source $genodeEnv/setup

runHook m4EnvSetup
mkdir $out

for src in $sources; do
    base=$(basename "$src")

    MSG_ASSEM $base
    object="${base%.asm}.o"

    VERBOSE $mpnDir/m4-ccas \
            --m4=m4 $cc $ccMarch -std=gnu99 -fPIC -DPIC \
            -c "$src" -o "$out/$object"
done
