{ genodeEnv, compileCC }:


genodeEnv.mkLibrary {
  name = "ldso-arch";

  objects = map (src: compileCC { inherit src; }) (
    if genodeEnv.isLinux then
      [ ./linux/parent_cap.cc ./linux/binary_name.cc ]
    else
    if genodeEnv.isNOVA then
      [ ./nova/parent_cap.cc ./binary_name.cc ]
    else
    throw "ldso-arch expression incomplete for ${genodeEnv.system}"
  );
}
