{ genodeEnv }:

genodeEnv.mkLibrary {
  name = "ldso-arch";
  src = [ "parent_cap.cc" "binary_name.cc" ];
  vpath = { "*" = if genodeEnv.isLinux then ./linux else ../arch; };
}