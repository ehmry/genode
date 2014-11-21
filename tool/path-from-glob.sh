cd $dir

for fn in $glob
do list="$list  $path/$fn\n"
done

echo -e '['$list']' > $out
