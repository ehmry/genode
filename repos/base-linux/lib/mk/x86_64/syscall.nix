{ sourceDir }:

{
  sources =
    map (fn: "${sourceDir}/platform/x86_64/${fn}")
      [ "lx_clone.S" "lx_restore_rt.S" "lx_syscall.S" ];
}