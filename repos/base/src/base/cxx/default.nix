{ build }:

# no library dependencies
{ }:

build.library rec {
  name = "cxx";
  shared = false;

  sources = 
    [ ./exception.cc ./guard.cc ./malloc_free.cc
      ./misc.cc ./new_delete.cc ./unwind.cc
    ];

  phases = [ "mergePhase" ];

  #
  # Here we define all symbols we want to hide in libsupc++ and libgcc_eh
  #
  libcSymbols = [
    "malloc" "free" "calloc" "realloc"
    "abort" "fputc" "fputs" "fwrite"
    "stderr" "stract" "strcpy" "strlen"
    "memcmp" "strncmp" "strcmp" "sprinf"
    "__stderrp"
  ];

  ehSymbols = [
    # Symbols we wrap (see unwind.cc)
    "_Unwind_Resume" "_Unwind_Complete" "_Unwind_DeleteException" 
  ] ++ [ # Additional symbols for ARM
    "__aeabi_unwind_cpp_pr0" "__aeabi_unwind_cpp_pr1"
  ];

  #
  # Dummy target used by the build system
  #
  localSymbols = map (s: "--localize-symbol="+s) libcSymbols;
  redefSymbols = map (s: "--redefine-sym ${s}=_cxx_${s} --redefine-sym __cxx_${s}=${s}") ehSymbols;

  #
  # Prevent symbols of the gcc support libs from being discarded during 'ld -r'
  #
  keepSymbolArgs = map (s: "-u ${s}") [
    "__cxa_guard_acquire"
    "__dynamic_cast"
    "_ZTVN10__cxxabiv116__enum_type_infoE"
    "_ZN10__cxxabiv121__vmi_class_type_infoD0Ev"
    "_ZTVN10__cxxabiv119__pointer_type_infoE"
    "_ZTSN10__cxxabiv120__function_type_infoE"
  ];

  #
  # Include dependency files for the corresponding object files except
  # when cleaning.
  #
  # Normally, the inclusion of dependency files is handled by 'generic.mk'.
  # However, the mechanism in 'generic.mk' considers only the dependencies
  # for the compilation units contained in the 'OBJECTS' variable. For building
  # the cxx library, we rely on the 'CXX_OBJECTS' variable instead. So we need to
  # include the dependenies manually.
  #
  #ifneq ($(filter-out $(MAKECMDGOALS),clean),)
  #-include $(CXX_OBJECTS:.o=.d)
  #endif

  # TODO check if these must be passed like this
  inherit (build.spec) ccMarch ldMarch;
  mergePhase = ''
    outName=subc++.o
  
    libcc_gcc="$($cxx $ccMarch -print-file-name=libsupc++.a) $($cxx $ccMarch -print-file-name=libgcc_eh.a)"

    MSG_MERGE $outName
    VERBOSE $ld $ldMarch $keepSymbolArgs -r $objects $libcc_gcc -o $outName.tmp

    mkdir -p $out

    MSG_CONVERT $outName
    VERBOSE $objcopy $localSymbols $redefSymbols $outName.tmp $out/cxx.lib.a
  '';

}