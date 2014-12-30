{ spec }:

with spec;

[ ( if is32Bit then ./32bit else
    if is64Bit then ./64bit else
    throw "NOVA does not support ${toString spec.bits}bit hardware"
  )
  ../include
]
