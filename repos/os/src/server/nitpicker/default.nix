{ genodeEnv, base, blit, config, server }:

genodeEnv.mkComponent {
  name = "nitpicker";
  libs = [ base blit config server ];
  sources = genodeEnv.fromPaths [
    ./main.cc
    ./view_stack.cc ./view.cc
    ./user_state.cc ./global_keys.cc
  ];

  binaries = [ ./default.tff ];
}
