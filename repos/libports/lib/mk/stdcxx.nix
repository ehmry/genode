{ genodeEnv, linkSharedLibrary, compileCCRepo, stdcxxSrc, libc  }:

let
  internalInclude = ../../include/stdcxx;
  externalInclude = stdcxxSrc.include + "/stdcxx";
in
linkSharedLibrary rec {
  name = "stdcxx";

  libs = [ libc ];

  externalObjects = compileCCRepo {
    inherit libs;
    sourceRoot = stdcxxSrc;
    sources =
      [ # libstdc++ sources
        "src/c++98/[!hash-long-double-tr1-aux][!strstream]*.cc"
        "src/c++11/*.cc"

        # config/locale/generic sources
        "config/locale/generic/*.cc"

        # config/os/generic sources
        "config/os/generic/*.cc"

        # config/io backend
        "config/io/basic_file_stdio.cc"

        # bits of libsupc++
        # (most parts are already contained in the cxx library)
      ] ++
      ( map
        (src: "libsupc++/"+src)
        [ "new_op.cc" "new_opnt.cc" "new_opv.cc" "new_opvnt.cc" "new_handler.cc"
          "del_op.cc" "del_opnt.cc" "del_opv.cc" "del_opvnt.cc"
          "bad_cast.cc" "bad_alloc.cc" "bad_typeid.cc"
          "eh_aux_runtime.cc" "hash_bytes.cc"
          "tinfo.cc"
        ]
      );

    systemIncludes =
      [ internalInclude
        externalInclude
        "${internalInclude}/bits"
        "${externalInclude}/bits"
        "${externalInclude}/std"
        "${externalInclude}/c_std"
        "${externalInclude}/c_global"

        "${stdcxxSrc.source}/libsupc++"
      ];

    extraFlags =
      [ "-D__GXX_EXPERIMENTAL_CXX0X__" "-std=c++11"

        # TODO: the following flags should propagate

        # prevent gcc headers from defining mbstate
        "-D_GLIBCXX_HAVE_MBSTATE_T"

        # use compiler-builtin atomic operations
        "-D_GLIBCXX_ATOMIC_BUILTINS_4"
      ];
  };

}
