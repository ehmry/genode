sources_=($sources)
targets_=($targets)

##
# Recursively recreate a file tree
#
for ((i = 0; i < ${#targets_[@]}; i++)); do
    target="${targets_[$i]}"
    source="${sources_[$i]}"

    if [ -d "$source" ]; then
        mkdir -p $out/$target
        ln -s ${source}/* "$out/${target}/"
    else
        mkdir -p $out/$(dirname $target)
        ln -s "${source}" $out/"${target}"
    fi
done
