/*
 * \author Emery Hemingway
 * \date   2014-08-11
 */

{ tool, base, os }:

tool.run {
  name = "fpu";

  bootInputs = [
    base.core os.init base.test.fpu
    (builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="ROM"/>
			<service name="RAM"/>
			<service name="CPU"/>
			<service name="RM"/>
			<service name="CAP"/>
			<service name="PD"/>
			<service name="LOG"/>
			<service name="SIGNAL"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test">
			<binary name="test-fpu"/>
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
    '')
  ];

  /*
  config = tool.initConfig {
    parentProvides = [ "ROM" "RAM" "CPU" "RM" "CAP" "PD" "LOG" "SIGNAL" ];
    defaultRoute = [ [ "any-service" "parent" ] ];
    children =
      { test = 
        { binary = base.test.fpu.name;
          resources = [ { name="RAM"; quantum="10M"; } ];
        };
      };
  };
  */

  waitRegex  = "test done.*\n";
  waitTimeout = 20;
}