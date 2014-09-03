/*
 * \brief  Entry expression into the build system
 * \author Emery Hemingway
 * \date   2014-08-11
 *
 * This file will change as the details shake out.
 *
 */

{ spec }:

let

  nixpkgs = import <nixpkgs> { };

  toolchain = import ./tool/toolchain {
    inherit spec nixpkgs;
  };

  tool = {
    inherit nixpkgs;

    build = import ./tool/build {
      inherit spec nixpkgs;
      toolchain = toolchain.precompiled;
    };

    bootImage = import ./tool/boot-image { inherit nixpkgs; };

    initConfig = import ./tool/init-config { inherit nixpkgs; };
    run =
      if spec.kernel == "linux" then
        import ./repos/base-linux/tool/run { inherit spec nixpkgs tool; } else
     if spec.kernel == "nova" then
        import ./repos/base-nova/tool/run { inherit spec nixpkgs tool; } else
     abort "no run environment for ${spec.system}";

     novaIso = import ./repos/base-nova/tool/iso { inherit nixpkgs tool; };

  };

  repos = rec {
    # these are sets with attributes that other repositories use,
    # but also attributes we don't want in the final expression
    base = import ./repos/base { inherit tool os; };
    os   = import ./repos/os   { inherit tool base; };
    demo = import ./repos/demo { inherit tool base os; };
    libports = import ./repos/libports { inherit tool base os; };
  };

  base      = repos.base.merge     repos;
  os       = repos.os.merge       repos;
  demo     = repos.demo.merge     repos;
  libports = repos.libports.merge repos;

in
(base // os // demo // libports // {

  test = base.test // os.test // demo.test;
  run  = base.run // os.run;



  #####################################################
  ##### this is quite a hack but atleast it works #####
  #####################################################

  demoImage = if spec.kernel == "nova" then tool.novaIso {
    name = "demo";

    inherit (base) hypervisor;

    genodeImage = tool.bootImage {
      name = "demo";
      inputs = [
        base.core os.init 
        os.driver.timer
        os.server.nitpicker
        demo.server.liquid_framebuffer
        demo.app.launchpad demo.app.scout
        os.test.nitpicker demo.server.nitlog

        libports.driver.framebuffer.vesa
        os.driver.pci os.driver.input.ps2
        
        (builtins.toFile "config" ''
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
	<start name="nitpicker">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Nitpicker"/></provides>
		<config>
			<global-keys>
				<key name="KEY_SCROLLLOCK" operation="xray" />
				<key name="KEY_SYSRQ"      operation="kill" />
				<key name="KEY_PRINT"      operation="kill" />
				<key name="KEY_F11"        operation="kill" />
				<key name="KEY_F12"        operation="xray" />
			</global-keys>
		</config>
	    </start>
	    <start name="launchpad">
		<resource name="RAM" quantum="64M" />
		<configfile name="launchpad.config" />
	    </start>
          </config>
        '')

        (builtins.toFile "launchpad.config" ''
          <config>
          	<launcher name="testnit"   ram_quota="768K" />
                	<launcher name="scout"     ram_quota="41M" />
                        <launcher name="launchpad" ram_quota="6M">
                        <configfile name="launchpad.config" />
                </launcher>
                <launcher name="nitlog"    ram_quota="1M" />
                <launcher name="liquid_fb" ram_quota="7M" />
                	<config resize_handle="on" />
                <launcher name="nitpicker" ram_quota="1M" />
          </config>
        '')

      ];
    };
  } else { };
})