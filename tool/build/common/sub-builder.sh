source $common/setup

mkdir $out

sed -e "s^@common@^$common^g" \
    < $setup > $out/setup
