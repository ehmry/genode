{ spec, tool, run, pkgs }:

with pkgs;

let
  haveFeature = feature: builtins.elem feature
    (spec.features or [ "ps2" "pci" "framebuffer" ]);

  featureComponent = feature: pkg:
    optionalFeature feature [] [ pkg ];

  featureConfig = feature: conf:
    optionalFeature feature "" conf;

  platform =
    if spec.isLinux then
      { config =
          ''
	<start name="fb_sdl">
		<resource name="RAM" quantum="4M"/>
		<provides>
			<service name="Input"/>
			<service name="Framebuffer"/>
		</provides>
	</start>
          '';
      } else
    if spec.isNOVA then
      { config =
          ''
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
          '';
      } else
    throw "Expression incomplete for ${spec.system}";

in
run {
  name = "sdl";
  automatic = false;

  contents = [
    { target = "/"; source = test.sdl; }
    { target = "/"; source = drivers.timer; }
    { target = "/"; source = drivers.framebuffer; }
    { target = "/"; source = drivers.pci; }
    { target = "/"; source = drivers.input.ps2; }
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
${platform.config}
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
      run_genode_until forever
    '';
}
