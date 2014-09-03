source $common/setup


symbol_name=$(echo $(basename $binary) | cut -c34-)
MSG_CONVERT $symbol_name
symbol_name=${symbol_name/.o//}
symbol_name=${symbol_name/binary_//}
symbol_name=${symbol_name/./_}
symbol_name=${symbol_name/-/_}
symbol_name=_binary_${symbol_name}


if [ -n  "$verbose" ]; then
    echo \
        echo ".global ${symbol_name}_start, ${symbol_name}_end; .data; .align 4; ${symbol_name}_start:; .incbin \"$binary\"; ${symbol_name}_end:" \
    \| $as $asOpt -f -o $out -
fi

echo ".global ${symbol_name}_start, ${symbol_name}_end; .data; .align 4; ${symbol_name}_start:; .incbin \"$binary\"; ${symbol_name}_end:" \
    | $as $asOpt -f -o $out -
