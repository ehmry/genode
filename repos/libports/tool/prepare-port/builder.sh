export PATH=
for i in $initialPath; do
    if [ "$i" = / ]; then i=; fi
    PATH=$PATH${PATH:+:}$i/bin
done

mkdir $out

sed -e "s^@initialPath@^$initialPath^g" \
    < $setup > $out/setup
