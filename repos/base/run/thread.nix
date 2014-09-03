/*
 * \author Emery Hemingway
 * \date   2014-08-11
 */

{ tool, base, os }:

tool.run {
  name = "thread";

  bootInputs = [
    base.core os.init base.test.thread
    (builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="LOG"/>
			<service name="RM"/>
			<service name="CPU"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test-thread">
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
    '')
  ];

  /*
  config = tool.initConfig {
    parentProvides = [ "LOG" "RM" "CPU" ];
    
    defaultRoute = [ [ "any-service" "parent" "any-child" ] ];

    children = {
      "test-thread" =
        { binary = base.test.thread.name;
          resources = [ { name="RAM"; quantum="10M"; } ];
        };
    };
  };
  */

  qemuArgs = [ "-nographic" "-m 64" ];

  waitRegex = "child exited with exit value 0.*\n";
  waitTimeout = 20;
}