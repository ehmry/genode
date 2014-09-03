export PATH=
for i in $initialPath; do
    if [ "$i" = / ]; then i=; fi
    PATH=$PATH${PATH:+:}$i/bin
done

mkdir $out

sed -e "s^@initialPath@^$initialPath^g" \
    -e "s^@toolchain@^$toolchain^g" \
    -e "s^@shell@^$shell^g" \
    < $setup > $out/setup

mkdir $out/nix-support
echo $propagatedUserEnvPkgs > $out/nix-support/propagated-user-env-packages
