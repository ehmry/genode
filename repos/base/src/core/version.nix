let
  genodeVersion = builtins.readFile ../../../../VERSION;
in
{
  ccOpt_version = ''-DGENODE_VERSION="\"${genodeVersion}\""'';
  sources = [ [ ./version.cc "version.cc" ] ];
}
