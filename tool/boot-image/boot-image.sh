##
# Recursively recreate a file tree
#
include() {
    for x in $1/*; do
        if [ -d "$x" ]; then
            local subdir="$2/$(basename \"$x\")"
            mkdir -p "$subdir"
            includeInput "$x" "$subdir"
        else
            ln -s "$x" "$2/"
        fi
    done
}

mkdir $out

for input in $inputs; do
        if [ -d $input ]; then
            include $input $out
        else
            name=
            ln -s $input $out/$(basename $input | cut -c34-)
        fi
done
