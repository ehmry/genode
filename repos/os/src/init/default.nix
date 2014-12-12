{ linkComponent, compileCC, base, init_pd_args, config }:

linkComponent {
  name = "init";
  libs = [ base init_pd_args config ];
  objects = compileCC { src = ./main.cc; };
}
