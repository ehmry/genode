#
# \brief  Simple test for Nix.
# \author Emery Hemingway
# \date   2015-01-11
#
# The test evaluates a very simple expression.
#

{ tool, run, pkgs }:

with pkgs;

run {
  name = "nix-eval";

  contents =
    [ { target = "/"; source = drivers.timer; }
      { target = "/"; source = drivers.rtc; }
      { target = "/"; source = test.nix-eval; }
      { target = "/config";
        source = builtins.toFile "config"
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
		<provides> <service name="Timer"/> </provides>
	</start>

	<start name="rtc_drv">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Rtc"/> </provides>
	</start>

	<start name="nix-eval">
		<resource name="RAM" quantum="4M"/>
		<config verbosity="6" show-stats="true">
			<libc stdout="/dev/log" stderr="/dev/log">
				<vfs>
					<tar name="nix.tar"/>
					<dir name="dev"> <log/> </dir>
				</vfs>
			</libc>
		</config>
	</start>
</config>
        '';
      }
      { target = "/nix.tar";
        source = tool.shellDerivation {
          name = "nix.tar";
          tar = "${tool.nixpkgs.gnutar}/bin/tar";
          inherit (app.nix) corepkgs;
          inherit (tool) genodeEnv;
          script = builtins.toFile "nix.tar.sh"
            ''
              source $genodeEnv/setup
              echo builtins.trace \"trace: --- Hello $\{builtins.currentSystem} world! ---\" null \
               	> default.nix
              mkdir nix; cp -r $corepkgs/corepkgs nix/
              $tar cfh $out default.nix nix
            '';
        };
      }
    ];

  testScript =
    ''
      append qemu_args "-m 64 -nographic"
      run_genode_until "child \"nix-eval\" exited with exit value 0" 10
    '';
}