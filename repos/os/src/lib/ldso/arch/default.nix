{ genodeEnv }:

genodeEnv.mkLibrary {
  name = "ldso-arch";
  sources = genodeEnv.fromDir
    (if genodeEnv.isLinux then ./linux else ../arch)
    [ "parent_cap.cc" "binary_name.cc" ];
}
