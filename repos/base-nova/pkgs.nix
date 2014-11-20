/*
 * \brief  Expression for the NOVA implementation of base packages
 * \author Emery Hemingway
 * \date   2014-09-16
 */

{ importBaseComponent }:
rec {
  core = importBaseComponent ./src/core;

  hypervisor = importBaseComponent ./src/kernel;
  kernel = hypervisor;
}
