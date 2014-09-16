{ build }:
{ base, blit, config, server }:

build.component {
  name = "nitpicker";
  libs = [ base blit config server ];
  sources = 
    [ ./main.cc
      ./view_stack.cc ./view.cc
      ./user_state.cc ./global_keys.cc
    ];

  binaries = [ ./default.tff ];
  includeDirs = [ ../nitpicker ];
}