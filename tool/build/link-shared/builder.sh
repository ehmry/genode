source $common/setup

mkdir $out

sed -e "s^@common@^$common^g" \
    < $script > $out/link-shared.sh
