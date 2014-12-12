source $genodeEnv/setup

runHook preLink

MSG_MERGE $name

objects=$(sortDerivations $objects)

for o in $externalObjects
do objects="$objects $o/*.o"
done

mkdir -p $out

VERBOSE $ar -rc $out/$name.lib.a $objects

runHook postLink
