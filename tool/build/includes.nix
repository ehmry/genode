{ toolchain }:
[
  ../../repos/base/include
  #builtins.toPath "${toolchain}/lib/gcc/${cross.target}/${toolchain.version}/include"
]

++ (
    [ ../../repos/base/include/x86
    ]
   )
++ (
    [ ../../repos/base/include/64bit
    ]
   )

++ # Find a way to switch between these
    [ ../../repos/base-linux/include
    ]
++
    [ ../../repos/base/include/x86_64
    ]