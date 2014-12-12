{ linkComponent, compileCC, repoDir, base }:

let platformInclude = repoDir+"/src/test/sub_rm"; in
linkComponent {
  name = "test-sub_rm";
  libs = [ base ];
  objects = compileCC {
    src = ./main.cc;
    systemIncludes =
      if builtins.pathExists platformInclude
      then [ platformInclude ]
      else [ ../sub_rm ];
  };
}
