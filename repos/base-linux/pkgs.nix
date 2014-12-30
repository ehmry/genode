/*
 * \brief  Expression for the Linux implementation of base packages
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ importBaseComponent }:

let
  lxHybridPreStart = "source ${./lx_hybrid_preStart.sh}";

in
{
  core = importBaseComponent ./src/core;

  test =
    { lx_hybrid_ctors       = importBaseComponent ./src/test/lx_hybrid_ctors;
      lx_hybrid_errno       = importBaseComponent ./src/test/lx_hybrid_errno;
      lx_hybrid_exception   = importBaseComponent ./src/test/lx_hybrid_exception;
      lx_hybrid_pthread_ipc = importBaseComponent ./src/test/lx_hybrid_pthread_ipc;
      rm_session_mmap       = importBaseComponent ./src/test/rm_session_mmap;
    };
}
