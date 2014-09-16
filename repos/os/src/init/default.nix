{ build }:

{ base, init_pd_args, config }:

build.component {
  name = "init";
   libs = [ base init_pd_args config ];
   sources = [ ./main.cc ];
}