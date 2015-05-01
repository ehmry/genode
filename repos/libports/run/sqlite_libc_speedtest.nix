/*
 * \brief  SQLite performance test
 * \author Emery Hemingway
 * \date   2015-01-31
 *
 * This test is used by SQLite upstream for regression testing.
 */

{ runtime, pkgs, ... }:

runtime.test {
  name = "sqlite_libc_speedtest";
  automatic = false; # Shared libraries

  components = with pkgs;
    [ test.sqlite_libc_speedtest
      drivers.timer
      server.ram_fs
    ];

  config = builtins.toFile
    "config.xml"
    ''
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
          <start name="ram_fs">
            <resource name="RAM" quantum="80M"/>
            <provides><service name="File_system"/></provides>
            <config>
              <policy label="" root="/" writeable="yes" />
            </config>
          </start>
        <start name="test-sqlite_libc_speedtest">
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

  testScript = ''
    append qemu_args " -nographic -m 256 "
    run_genode_until {.*child "test-sqlite_libc_speedtest" exited with exit value 0.*} 600
  '';
}
