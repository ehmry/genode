/*
 * \brief  Basic test for genode timer-session
 * \author Martin Stein
 * \date   2012-05-29
 */

{ tool, base, os }:

tool.run {
  name = "timer";

  bootInputs = [
    base.core os.init os.driver.timer os.test.timer
    (builtins.toFile "config" ''
      <config>
	<parent-provides>
		<service name="ROM"/>
		<service name="RAM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="CAP"/>
		<service name="PD"/>
		<service name="RM"/>
		<service name="CPU"/>
		<service name="LOG"/>
		<service name="SIGNAL"/>
	</parent-provides>
	<default-route>
		<any-service><parent/><any-child/></any-service>
	</default-route>
	<start name="timer">
		<resource name="RAM" quantum="10M"/>
		<provides><service name="Timer"/></provides>
	</start>
	<start name="client">
		<binary name="test-timer"/>
		<resource name="RAM" quantum="10M"/>
	</start>
      </config>
    '')
  ];

  /*
  config = tool.initConfig {
    parentProvides =
      [ "ROM" "RAM" "CPU" "RM" "CAP" "PD" "LOG" "SIGNAL" ];

    defaultRoute = [ [ "any-service" "parent" "any-child" ] ];

    children = {
      "timer" =
        { binary = os.driver.timer.name;
          resources = [ { name="RAM"; quantum="10M"; } ];
          provides  = [ { name="Timer"; } ];
        };
      "client" =
        { binary = os.test.timer.name;
          resources = [ { name="RAM"; quantum="10M"; } ];
        };
    };
  };
  */

  waitRegex = "--- timer test finished ---";
  waitTimeout = 60;
}