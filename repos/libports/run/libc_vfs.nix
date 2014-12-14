#
# \brief  Test for using the libc_vfs plugin
# \author Norman Feske
# \date   2014-04-10
#

{ tool, run, pkgs }:

with pkgs;

run {
  name = "libc_vfs";

  contents =
    [ { target = "/"; source = libs.libc; }
      { target = "/"; source = server.ram_fs; }
      { target = "/"; source = test.libc_vfs; }
      { target = "/config";
        source = builtins.toFile "config"
          ''
          <config>
	<parent-provides>
		<service name="ROM"/>
		<service name="RAM"/>
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
	<start name="ram_fs">
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="File_system"/> </provides>
		<config> <policy label="" root="/" writeable="yes" /> </config>
	</start>
	<start name="test-libc_vfs">
		<resource name="RAM" quantum="2M"/>
		<config>
			<iterations value="1"/>
			<libc stdout="/dev/log" cwd="/tmp" >
				<vfs>
					<dir name="tmp" >
						<fs/>
					</dir>
					<dir name="dev"> <log/> </dir>
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
      append qemu_args " -m 128 -nographic "
      run_genode_until {.*child "test-libc_vfs" exited with exit value 0.*} 60
    '';
}
