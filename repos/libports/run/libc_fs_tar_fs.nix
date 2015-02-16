#
# \brief  Test for using the libc_vfs plugin with the TAR file system
# \author Christian Prochaska
# \date   2012-08-20
#



{ tool, run, pkgs }:

run {
  name = "libc_fs_tar_fs";

  contents =
    [ { target = "/"; source = pkgs.drivers.timer; }
      { target = "/"; source = pkgs.server.tar_fs; }
      { target = "/"; source = pkgs.libs.libc; }

      { target = "/"; source = pkgs.test.libc_fs_tar_fs; }
      # The test archive comes with test.libc_fs_tar_fs.

      { target = "/config";
        source = builtins.toFile "config"
          ''
          <config>
            <parent-provides>
		<service name="ROM"/>
		<service name="RAM"/>
		<service name="CAP"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
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
	<start name="tar_fs">
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="File_system"/> </provides>
		<config>
			<archive name="libc_fs_tar_fs.tar" />
			<policy label="" root="/testdir" />
		</config>
	</start>
	<start name="test-libc_fs_tar_fs">
		<resource name="RAM" quantum="2M"/>
		<config>
			<libc stdout="/dev/log">
				<vfs>
					<dir name="dev"> <log/> </dir>
					<fs/>
				</vfs>
			</libc>
		</config>
	</start>
        </config>
        '';
      }
    ];

  testScript =
    ''
      #
      # Qemu
      #
      append qemu_args " -m 128 -nographic "

      run_genode_until {.*child "test-libc_fs_tar_fs" exited with exit value 0.*} 60

      puts "\nTest succeeded\n"
    '';
}
