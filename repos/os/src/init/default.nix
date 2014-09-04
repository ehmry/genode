{ build, base, os }:

build.component {
  name = "init";

   libs =
     [ base.lib.base ]
     ++
     (with os.lib; [ init_pd_args config ]);

   sources = [ ./main.cc ];

   includeDirs =
     os.includeDirs
     ++
     base.includeDirs;
}