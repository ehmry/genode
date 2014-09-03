/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ tool, base, os }:

tool.run {
  name = "signal";

  bootInputs = [
    base.core os.init os.driver.timer os.test.signal
    (builtins.toFile "config" ''
    	<config>
		<parent-provides>
			<service name="ROM"/>
			<service name="RAM"/>
			<service name="CPU"/>
			<service name="RM"/>
			<service name="CAP"/>
			<service name="PD"/>
			<service name="IRQ"/>
			<service name="IO_PORT"/>
			<service name="IO_MEM"/>
			<service name="SIGNAL"/>
			<service name="LOG"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> <any-child/> </any-service>
		</default-route>
		<start name="timer">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Timer"/></provides>
		</start>
		<start name="test-signal">
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
    '')
  ];

  /*
  config = tool.initConfig {
    parentProvides =
      [ "ROM" "RAM" "CPU" "RM" "CAP" "PD" "IRQ" "IO_PORT" "IO_MEM" "SIGNAL" "LOG" ];

    defaultRoute = [ [ "any-service" "parent" "any-child" ] ];

    children = {
      "timer" =
        { binary = os.driver.timer.name;
          resources = [ { name="RAM"; quantum="10M"; } ];
          provides  = [ { name="Timer"; } ];
        };

      "test-signal" =
        { binary = os.test.signal.name;
          resources = [ { name="RAM"; quantum="10M"; } ];
        };
    };
  };
  */

  waitRegex = "child exited with exit value 0.*";
  waitTimeout = 200;
}