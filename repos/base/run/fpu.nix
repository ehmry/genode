/*
 * \author Emery Hemingway
 * \date   2014-08-11
 */

{ run }:
{ fpu }:

run {
  name = "fpu";

  contents = [
    { target = "/"; source = fpu; }
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
      '';
    }
  ];

  waitRegex  = "test done.*\n";
  waitTimeout = 20;
}