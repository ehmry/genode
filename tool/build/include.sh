absolute=($absolute)
relative=($relative)
for ((n = 0; n < ${#relative[*]}; n += 1))
do echo ${absolute[n]} ${relative[n]} >> $out
done