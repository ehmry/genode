{ spec, tool, run, pkgs }:

with pkgs;

let

  rawSample = tool.shellDerivation {
    name = "sample.raw";
    inherit (tool.nixpkgs) sox;
    script = builtins.toFile "raw-sample.sh"
      ''    
        $sox/bin/sox -c 2 -r 44100 -n $out \
	    synth 2.5 sin 667 gain 1 \
	    bend .35,180,.25  .15,740,.53  0,-520,.3
      '';
  };

in

if spec.isx86_32 then run {
  name = "oss";

  contents =
    [ { target = "/"; source = driver.acpi; }
      { target = "/"; source = driver.audio_out; }
      { target = "/"; source = driver.pci; }
      { target = "/"; source = driver.pci_device_pd; }
      { target = "/"; source = driver.timer; }
            
      { target = "/"; source = test.audio_out; }
      { target = "/sample.raw";
        source = rawSample;
      }

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
		<service name="SIGNAL" />
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<start name="acpi">
		<resource name="RAM" quantum="4M"/>
		<binary name="acpi_drv"/>
		<provides>
			<service name="PCI"/>
			<service name="IRQ" />
		</provides>
		<route>
			<service name="PCI"> <any-child /> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>
	<start name="audio_out_drv">
		<resource name="RAM" quantum="6M"/>
		<route>
			<service name="IRQ"><child name="acpi" /></service>
			<any-service> <parent /> <any-child /></any-service>
		</route>
		<provides>
			<service name="Audio_out"/>
		</provides>
	</start>
	<start name="audio0">
		<binary name="test-audio_out"/>
		<resource name="RAM" quantum="8M"/>
		<config>
			<filename>sample.raw</filename>
		</config>
		<route>
			<service name="Audio_out"> <child name="audio_out_drv"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
</config>
'';
    }
  ];

  testScript =
    ''
      append qemu_args " -m 256 -soundhw ac97 -nographic"

      run_genode_until forever
    '';

} else null
