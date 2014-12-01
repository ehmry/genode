{ spec, run, pkgs }:

let
  haveFeature = feature: builtins.elem feature
    (spec.features or [ "ps2" "pci" "framebuffer" ]);

  optionalFeature = feature: empty: x:
    if (haveFeature feature) then x else empty;

  featureComponent = feature: pkg:
    optionalFeature feature [] [ pkg ];

  featureConfig = feature: conf:
    optionalFeature feature "" conf;

  result = run {
  name = "demo";

  contents = with pkgs;
    (map (pkg: { target = "/"; source = pkg; })
     ([ core init
        driver.timer
        server.nitpicker app.pointer app.status_bar
	server.liquid_framebuffer app.launchpad app.scout
	test.nitpicker server.nitlog
	driver.framebuffer driver.pci driver.input.ps2
	server.report_rom
      ] ++
      ( featureComponent "usb"          driver.usb ) ++
      ( featureComponent "gpio"         driver.gpio ) ++
      ( featureComponent "imx53"        driver.platform ) ++
      ( featureComponent "exynos5"      driver.platform ) ++
      ( featureComponent "platform_rpi" driver.platform )
     )
    ) ++
    [ { target = "/config";
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

          ${featureConfig "sdl" ''
          <start name="fb_sdl">
            <resource name="RAM" quantum="4M"/>
            <provides>
              <service name="Input"/>
              <service name="Framebuffer"/>
            </provides>
          </start>
          ''}

          ${featureConfig "pci" ''
          <start name="pci_drv">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="PCI"/></provides>
          </start>
          ''}

          ${featureConfig "framebuffer" ''
          <start name="fb_drv">
            <resource name="RAM" quantum="4M"/>
            <provides><service name="Framebuffer"/></provides>
          </start>
          ''}

          ${featureConfig "gpio" ''
          <start name="gpio_drv">
            <resource name="RAM" quantum="4M"/>
            <provides><service name="Gpio"/></provides>
            <config/>
          </start>
          ''}

          ${featureConfig "exynos5" ''
          <start name="platform_drv">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Regulator"/></provides>
            <config/>
          </start>
          ''}

          ${featureConfig "platform_rpi" ''
          <start name="platform_drv">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Platform"/></provides>
            <config/>
          </start>
          ''}

          ${featureConfig "imx53" ''
          <start name="platform_drv">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Platform"/></provides>
            <config/>
          </start>
          <start name="input_drv">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Input"/></provides>
            <config/>
          </start>
          ''}

          ${featureConfig "ps2" ''
          <start name="ps2_drv">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Input"/></provides>
          </start>
          ''}

          ${if (haveFeature "ps2") && (haveFeature "usb") then ''
          <start name="usb_drv">
            <resource name="RAM" quantum="12M"/>
            <provides><service name="Input"/></provides>
            <config ehci="yes" uhci="yes" xhci="no"> <hid/> </config>
          </start>
          '' else ""}

          <start name="timer">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Timer"/></provides>
          </start>
          <start name="report_rom">
            <resource name="RAM" quantum="1M"/>
            <provides> <service name="Report"/> <service name="ROM"/> </provides>
            <config>
              <rom>
                <policy label="status_bar -> focus" report="nitpicker -> focus"/>
              </rom>
            </config>
          </start>
          <start name="nitpicker">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Nitpicker"/></provides>
            <config>
              <report focus="yes" />
              <domain name="pointer" layer="1" xray="no" origin="pointer" />
              <domain name="panel"   layer="2" xray="no" />
              <domain name=""        layer="3" ypos="18" height="-18" />

              <policy label="pointer"    domain="pointer"/>
              <policy label="status_bar" domain="panel"/>
              <policy label=""           domain=""/>

              <global-key name="KEY_SCROLLLOCK" operation="xray" />
              <global-key name="KEY_SYSRQ"      operation="kill" />
              <global-key name="KEY_PRINT"      operation="kill" />
              <global-key name="KEY_F11"        operation="kill" />
              <global-key name="KEY_F12"        operation="xray" />
            </config>
          </start>
          <start name="pointer">
            <resource name="RAM" quantum="1M"/>
          </start>
          <start name="status_bar">
            <resource name="RAM" quantum="1M"/>
            <route>
              <service name="ROM"> <child name="report_rom"/> </service>
              <any-service> <parent/> <any-child/> </any-service>
            </route>
          </start>
          <start name="launchpad">
            <resource name="RAM" quantum="64M" />
            <configfile name="launchpad.config" />
          </start>
        </config>
      '';
    }

    # Create launchpad configuration
    { target = "/launchpad.config";
      source = builtins.toFile "launchpad.config" ''
        <config>
          <launcher name="testnit"   ram_quota="768K" />
          <launcher name="scout"     ram_quota="41M" />
          <launcher name="launchpad" ram_quota="6M">
            <configfile name="launchpad.config" />
          </launcher>
          <launcher name="nitlog"    ram_quota="1M" />
          <launcher name="liquid_fb" ram_quota="7M">
            <config resize_handle="on" />
          </launcher>
          <launcher name="nitpicker" ram_quota="1M">
            <config>
              <domain name="" layer="3" />
              <policy label="" domain="" />
            </config>
          </launcher>
        </config>
      '';
    }

  ];

  testScript =
    ''
      append qemu_args " -m 256 "
    '';
};

in result.diskImage
