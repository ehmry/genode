#
# \brief  Test for using the libc_vfs plugin with the Linux file system
# \author Norman Feske
# \author Christian Helmuth
# \date   2013-11-07
#

{ spec, tool, run, pkgs }:

with pkgs;

if !spec.isLinux then null else

run {
  name = "lx_fs";

  contents =
    ( map
        ( source: { target = "/"; inherit source; })
        [ server.lx_fs test.libc_vfs ]
    ) ++
    [ { target = "/config";
        source = builtins.toFile "config.xml"
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
	<start name="lx_fs">
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="File_system"/> </provides>
		<config> <policy label="" root="/libc_vfs" writeable="yes" /> </config>
	</start>
	<start name="test-libc_vfs">
		<resource name="RAM" quantum="2M"/>
		<config>
			<libc stdout="/dev/log">
				<vfs>
					<fs />
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
      file mkdir libc_vfs
      run_genode_until {child "test-libc_vfs" exited with exit value 0.*\n} 20
    '';
}