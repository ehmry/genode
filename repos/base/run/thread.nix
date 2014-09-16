/*
 * \author Emery Hemingway
 * \date   2014-08-11
 */

{ run }:
{ thread }:

run {
  name = "thread";

  contents = [
    { target = "/"; source = thread; }
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

  qemuArgs = [ "-nographic" "-m 64" ];

  waitRegex = "child exited with exit value 0.*\n";
  waitTimeout = 20;
}