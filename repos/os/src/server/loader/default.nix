{ linkComponent, compileCC, filterHeaders, base, init_pd_args }:

linkComponent {
  name = "loader";
  libs = [ base init_pd_args ];
  objects =
    [ (compileCC {
        src = ./main.cc;
        systemIncludes = [ (filterHeaders ../loader) ];
      })
    ];
}
