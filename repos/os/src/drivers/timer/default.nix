{ build, base, os }:

let

  platformLibs =
    if build.isLinux then [ base.lib.syscall ] else
    if build.isNova then [ ] else
    throw "no timer driver for ${build.spec.system}";

  platformSources =
    if build.isLinux then [ ./linux/platform_timer.cc ] else [];

  platformIncludes =
    if build.isLinux then [ ./include_periodic ] else
    if build.isNova then [ ./nova ] else
    throw "no timer driver for ${build.spec.system}";

in
build.driver {
  name = "timer";
  libs =
    [ base.lib.base os.lib.alarm ]
    ++ platformLibs;

  sources =
    [ ./main.cc
    ] ++ platformSources;

  includeDirs =
    platformIncludes
    ++ [ ./include ]
    ++ os.includeDirs
    ++ base.includeDirs;
}