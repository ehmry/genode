{ genodeEnv, compileCC, transformBinary, base, blit, config, server }:

genodeEnv.mkComponent {
  name = "nitpicker";
  libs = [ base blit config server ];
  objects =
    (map (src: compileCC { inherit src; })
      [ ./main.cc ./view_stack.cc ./view.cc
        ./user_state.cc ./global_keys.cc
      ]
    ) ++ [ (transformBinary ./default.tff) ];
}
