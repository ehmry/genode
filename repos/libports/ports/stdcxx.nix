{ genodeEnv, preparePort, fetchurl }:

let
  inherit (genodeEnv.toolchain) version;
in
preparePort {
  name = "stdcxx-${version}";

  src = fetchurl {
    url = "mirror://gnu/gcc/gcc-${version}/gcc-${version}.tar.bz2";
    sha256 = "10k2k71kxgay283ylbbhhs51cl55zn2q38vj5pk4k950qdnirrlj";
  };

  tarFlags = "gcc-${version}/libstdc++-v3 --strip-components=2";

  sourceRoot = ".";

  installPhase =
    ''
      mkdir -p $source $include/stdcxx/bits $include/stdcxx/config
      cp -r src config libsupc++ $source

      cp -r \
          include/* \
          libsupc++/new \
          libsupc++/exception \
          libsupc++/typeinfo \
          libsupc++/initializer_list \
          $include/stdcxx

      cp -r \
          libsupc++/hash_bytes.h \
          libsupc++/exception_defines.h \
          libsupc++/exception_ptr.h \
          libsupc++/nested_exception.h \
          libsupc++/cxxabi_forced.h \
          libsupc++/atomic_lockfree_defines.h \
          config/cpu/generic/cpu_defines.h \
          config/cpu/generic/cxxabi_tweaks.h \
          config/os/generic/error_constants.h \
          config/os/generic/os_defines.h \
          config/cpu/generic/atomic_word.h \
          config/os/generic/ctype_base.h \
          config/os/generic/ctype_inline.h \
          config/locale/generic/time_members.h \
          config/locale/generic/messages_members.h \
          $include/stdcxx/bits

      cp -r \
          config/allocator/new_allocator_base.h \
          config/locale/generic/c_locale.h \
          config/io/basic_file_stdio.h \
          config/io/c_io_stdio.h \
          $include/stdcxx/config
    '';

  outputs = [ "source" "include" ];

}
