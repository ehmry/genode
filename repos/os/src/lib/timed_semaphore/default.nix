{ linkStaticLibrary, compileCC, alarm }:

linkStaticLibrary {
  name = "timed_semaphore";
  libs = [ alarm ];
  objects = compileCC { src = ./timed_semaphore.cc; };
}
