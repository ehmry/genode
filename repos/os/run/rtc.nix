# RTC test

{ tool, run, pkgs }:

with pkgs;

run {
  name = "rtc";

  contents =
    [ { target = "/"; source = drivers.timer; }
      { target = "/"; source = drivers.rtc; }
      { target = "/"; source = test.rtc; }
      { target = "/config";
        source = builtins.toFile "config.xml"
          ''
<config prio_levels="2" verbose="yes">
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
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>
	<start name="rtc_drv" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Rtc"/></provides>
	</start>
	<start name="test-rtc" priority="-1">
		<resource name="RAM" quantum="1M"/>
	</start>
</config>
        '';
      }
    ];

  testScript =
    ''
      append qemu_args " -nographic -m 128 "
      run_genode_until ".*--- RTC test finished ---.*\n" 20
    '';
}
