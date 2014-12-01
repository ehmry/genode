source $genodeEnv/setup

MSG_CONVERT $symbolName

echo ".global ${symbolName}_start, ${symbolName}_end; .data; .align 4; ${symbolName}_start:; .incbin \"$binary\"; ${symbolName}_end:" | \
    $as $asOpt -f -o $out -
