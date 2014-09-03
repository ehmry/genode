{ build, base, os }:

build.server {
  name = "nitpicker";

  libs =
    [ base.lib.base
      os.lib.blit os.lib.config os.lib.server
    ];

  sources = 
    [ ./main.cc
      ./view_stack.cc ./view.cc
      ./user_state.cc ./global_keys.cc
    ];

  binaries = [ ./default.tff ];

  includeDirs = [ ../nitpicker ]
    ++ os.includeDirs ++ base.includeDirs;

}