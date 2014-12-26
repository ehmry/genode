{ run, pkgs }:

with pkgs;

let
  spec = tool.genodeEnv.spec;

  haveFeature = feature: builtins.elem feature
    (spec.features or [ "ps2" "pci" "framebuffer" ]);

  featureComponent = feature: pkg:
    optionalFeature feature [] [ pkg ];

  featureConfig = feature: conf:
    optionalFeature feature "" conf;
in
run {
  name = "sdl";

  contents = [
    { target = "/"; source = test.sdl; }
  ] ++
  # TODO: this is good, but move it into run or something
  ( map (lib: { target = "/"; source = lib; }) test.sdl.libs ) ++
  [
    { target = "/"; source = driver.timer; }
    { target = "/"; source = driver.framebuffer; }
    { target = "/"; source = driver.pci; }
    { target = "/"; source = driver.input.ps2; }
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
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<start name="pci_drv">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="PCI"/></provides>
	</start>
	<start name="fb_drv">
		<resource name="RAM" quantum="4M"/>
		<provides><service name="Framebuffer"/></provides>
	</start>
	<start name="ps2_drv">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Input"/></provides>
	</start>
	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Timer"/></provides>
	</start>
	<start name="test-sdl">
		<resource name="RAM" quantum="32M"/>
		<config>
			<libc stdout="/dev/log">
				<vfs> <dir name="dev"> <log/> </dir> </vfs>
			</libc>
		</config>
	</start>
        </config>
      '';
    }
  ];

  testScript =
    ''
      append qemu_args " -m 256 "
      run_genode_until "i don't know what" 20
    '';
}
