/*
 * \brief  SQLite performance test
 * \author Emery Hemingway
 * \date   2015-01-31
 *
 * This test is used by SQLite upstream for regression testing.
 */

{ run, pkgs }:

with pkgs;

run {
  name = "sqlite_speedtest";

  contents = [
    { target = "/"; source = test.sqlite_speedtest; }
    { target = "/"; source = drivers.timer; }
    { target = "/"; source = drivers.rtc; }
    { target = "/"; source = server.ram_fs; }
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
          <start name="timer">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Timer"/></provides>
          </start>
          <start name="rtc_drv">
             <resource name="RAM" quantum="1M"/>
            <provides><service name="Rtc"/></provides>
          </start>
          <start name="ram_fs">
            <resource name="RAM" quantum="80M"/>
            <provides><service name="File_system"/></provides>
            <config>
              <policy label="" root="/" writeable="yes" />
            </config>
          </start>
        <start name="test-sqlite_speedtest">
          <resource name="RAM" quantum="16M"/>
            <config>
              <libc stdout="/dev/log">
                <vfs>
                   <fs/>
                  <dir name="dev"> <log/> <null/> </dir>
                </vfs>
              </libc>
            </config>
          </start>
        </config>
      '';
    }
  ];

  testScript = ''
    append qemu_args " -nographic -m 256 "
    run_genode_until {.*child "test-sqlite_speedtest" exited with exit value 0.*} 600
  '';
}
