{ sourceDir }:

{
  sources =
    map (fn: "${sourceDir}/platform/arm/${fn}") 
      [ "lx_clone.S" "lx_syscall.S" ];
}