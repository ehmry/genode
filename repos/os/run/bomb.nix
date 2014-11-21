{ run, pkgs }:

run {
  name = "bomb";

  contents = [
    { target = "/"; source = pkgs.driver.timer; }
    { target = "/"; source = pkgs.test.bomb; }
    { target = "/config";
      source = builtins.toFile "config" ''
        <config prio_levels="2">
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
            <resource name="RAM" quantum="768K"/>
            <provides><service name="Timer"/></provides>
          </start>
          <start name="bomb" priority="-1">
            <resource name="RAM" quantum="2G"/>
            <config rounds="20"/>
          </start>
        </config>
      '';
    }
  ];

  testScript =
  ''
    append qemu_args "-nographic -m 128"
    run_genode_until "Done\. Going to sleep\." 300
    puts "Test succeeded."
  '';
}
