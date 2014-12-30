{ spec, tool, callComponent }:

let
  importComponent = path: callComponent { } (import path);
in
{
  driver.nic      = importComponent ./src/drivers/nic;
  driver.nic_stat = importComponent ./src/drivers/nic_stat;
}
