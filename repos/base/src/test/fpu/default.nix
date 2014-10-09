{ genodeEnv, base }:

genodeEnv.mkComponent {
  name = "test-fpu"; libs = [ base ]; src = [ ./main.cc ];
}