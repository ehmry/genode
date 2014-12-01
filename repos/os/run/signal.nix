/*
 * \author Emery Hemingway
 * \date   2014-08-12
 */

{ run, pkgs }:

with pkgs;

run {
  name = "signal";

  contents = [
    { target = "/"; source = driver.timer; }
    { target = "/"; source = test.signal; }
    { target = "/config";
      source = builtins.toFile "config" ''
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
      '';
    }
  ];

  testScript = ''
    append qemu_args "-nographic -m 64"
    run_genode_until {child "test-signal" exited with exit value 0.*} 200
    puts "Test succeeded"
  '';
}
