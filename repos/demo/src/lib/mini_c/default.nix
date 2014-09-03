{ build, base, os, demo }:

build.library {
  name = "mini_c";
  stdinc = true;
  sources =
    [ ./abort.cc       ./atol.cc
      ./malloc_free.cc ./memcmp.cc
      ./memset.cc      ./mini_c.c
      ./printf.cc      ./snprintf.cc
      ./strlen.cc      ./strtod.cc
      ./strtol.cc      ./vsnprintf.cc
    ];
  includeDirs = [ (demo.includeDir + "/mini_c") ] ++  base.includeDirs;
}