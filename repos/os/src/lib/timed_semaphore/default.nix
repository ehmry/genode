{ genodeEnv, compileCC, alarm }:

genodeEnv.mkLibrary {
  name = "timed_semaphore";
  libs = [ alarm ];
  objects = compileCC { src = ./timed_semaphore.cc; };
}
