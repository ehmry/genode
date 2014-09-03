{ sourceDir }:

{
  sources =
    map (fn: "${sourceDir}/platform/x86_32/${fn}")
      [ "lx_clone.S" "lx_syscall.S" ];
}