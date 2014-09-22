/*
 * \brief  Basic test for genode timer-session
 * \author Martin Stein
 * \date   2012-05-29
 */

{ run, pkgs }:

with pkgs;

run {
  name = "timer";

  contents = [
    { target = "/"; source = driver.timer; }
    { target = "/"; source = test.timer;   }
    { target = "/config";
      source = builtins.toFile "config" ''
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
      ''; 
    }
  ];

  testScript = ''
    # Configure Qemu
    append qemu_args " -m 64 -nographic"

    # Execute test in Qemu
    run_genode_until "--- timer test finished ---" 60

    puts "Test succeeded"
  '';

}