/*
 * \author Emery Hemingway
 * \date   2014-08-11
 */

{ run, pkgs }:

with pkgs;

run {
  name = "thread";

  contents = [
    { target = "/"; source = test.thread; }
    { target = "/config";
      source = builtins.toFile "config" ''
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
      '';
    }
  ];

  testScript = ''
    append qemu_args "-nographic -m 64"

    run_genode_until "child exited with exit value 0.*\n" 20

    puts "Test succeeded"
  '';

}