/*
 * \brief  Expression for the Linux implementation of base packages
 * \author Emery Hemingway
 * \date   2014-09-04
 */

{ importBaseComponent }:
{
  core = importBaseComponent ./src/core;

  test =
    { rm_session_mmap = importBaseComponent ./src/test/rm_session_mmap;
    };
}
