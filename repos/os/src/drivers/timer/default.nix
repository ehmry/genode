{ build }:
{ base, alarm, syscall }:

let

  platformLibs =
    if build.isLinux then [ syscall ] else
    if build.isNova then [ ] else
    throw "no timer driver for ${build.spec.system}";

  platformSources =
    if build.isLinux then [ ./linux/platform_timer.cc ] else [];

  platformIncludes =
    if build.isLinux then [ ./include_periodic ] else
    if build.isNova then [ ./nova ] else
    throw "no timer driver for ${build.spec.system}";

in
build.component {
  name = "timer";
  libs = [ base alarm ] ++ platformLibs;

  sources =
    [ ./main.cc
    ] ++ platformSources;

  includeDirs =
    platformIncludes
    ++
    [ ../../../../base-linux/src/platform ./include ];

}