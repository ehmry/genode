list="[\n"

for f in $dir/$glob
do list="$list  [$f \"${f##*/}\"]\n"
done

list="$list]"

echo -e $list > $out
