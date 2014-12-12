/*
 * \brief  Portions of base library that are exclusive to non-core processes
 * \author Norman Feske
 * \date   2013-02-14
 */

{ linkStaticLibrary, compileCC, baseDir, repoDir, baseIncludes
, base-common }:

let
  compileCC' = src: compileCC {
    inherit src;
    systemIncludes = [ (baseDir + "/src/base/env") ];
  };
in
linkStaticLibrary {
  name = "base";
  libs = [ base-common ];
  objects = map compileCC' (
    [ (repoDir + "/src/base/thread/thread_nova.cc") ]
    ++
    ( map (fn: baseDir+"/src/base/${fn}")
        [ "console/log_console.cc"
          "cpu/cache.cc"
          "env/context_area.cc"
          "env/env.cc"
          "env/reinitialize.cc"
        ]
    )
  );
  propagatedIncludes = baseIncludes;
}
