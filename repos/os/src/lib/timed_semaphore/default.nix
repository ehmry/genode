{ genodeEnv, alarm }:

genodeEnv.mkLibrary {
  name = "timed_semaphore";
  libs = [ alarm ];
  sources = genodeEnv.fromPath ./timed_semaphore.cc;
}
