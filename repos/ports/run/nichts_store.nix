#
# \brief  Brief nichts_store test
# \author Emery Hemingway
# \date   2015-03-13
#

{ runtime, pkgs, tool, ... }:

runtime.test {
  name = "nichts_store";

  components = with pkgs;
    [ drivers.timer server.ram_fs
      server.nichts_store test.nichts_client
      noux.minimal server.log_terminal
    ];


  config = builtins.toFile "nichts_store.config"
    ''
<config verbose="yes">
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
		<resource name="RAM" quantum="4M"/>
		<config>
			<policy label="" root="/" writeable="yes"/>
			<content>
				<dir name="nix">
					<dir name="store">
						<inline name="0xfzj12nd1si06xsh4hdn8bdg0pr689n-genode-toolchain-14.11-x86_64.tar.bz2.drv">Derive([("out","/nix/store/xnxx6mjaddl2xi6mj4ail8sy3wp786iw-genode-toolchain-14.11-x86_64.tar.bz2","sha256","05d1a0f02682febc58c041c349e60a0d07b5f80342b64a36ba03b34aedce5c76")],[("/nix/store/00pamshzwd113liqkg9b30wanvbg9zi6-bash-4.3-p30.drv",["out"]),("/nix/store/3yfgcdazaq73adcd5zwswbvvj7f36rib-curl-7.39.0.drv",["out"]),("/nix/store/gdvl2pw0hpqkg2162dapcxh1f07mwxw2-stdenv.drv",["out"]),("/nix/store/r1qvi7g2wggdydxa6jkzgy86hfwps0ha-mirrors-list.drv",["out"])],["/nix/store/xmlyix918zakfcz0b5c14y56fdda2a10-builder.sh"],"x86_64-linux","/nix/store/r5sxfcwq9324xvcd1z312kb9kkddqvld-bash-4.3-p30/bin/bash",["-e","/nix/store/xmlyix918zakfcz0b5c14y56fdda2a10-builder.sh"],[("buildInputs",""),("builder","/nix/store/r5sxfcwq9324xvcd1z312kb9kkddqvld-bash-4.3-p30/bin/bash"),("curlOpts",""),("downloadToTemp",""),("impureEnvVars","http_proxy https_proxy ftp_proxy all_proxy no_proxy NIX_CURL_FLAGS NIX_HASHED_MIRRORS NIX_CONNECT_TIMEOUT NIX_MIRRORS_apache NIX_MIRRORS_bitlbee NIX_MIRRORS_cpan NIX_MIRRORS_cran NIX_MIRRORS_debian NIX_MIRRORS_fedora NIX_MIRRORS_gcc NIX_MIRRORS_gentoo NIX_MIRRORS_gnome NIX_MIRRORS_gnu NIX_MIRRORS_gnupg NIX_MIRRORS_hackage NIX_MIRRORS_hashedMirrors NIX_MIRRORS_imagemagick NIX_MIRRORS_kde NIX_MIRRORS_kernel NIX_MIRRORS_metalab NIX_MIRRORS_oldsuse NIX_MIRRORS_opensuse NIX_MIRRORS_postgresql NIX_MIRRORS_roy NIX_MIRRORS_sagemath NIX_MIRRORS_savannah NIX_MIRRORS_sourceforge NIX_MIRRORS_sourceforgejp NIX_MIRRORS_ubuntu NIX_MIRRORS_xfce NIX_MIRRORS_xorg"),("mirrorsFile","/nix/store/bri7383wifl1g0g2n3f3qipfidi1ccb6-mirrors-list"),("name","genode-toolchain-14.11-x86_64.tar.bz2"),("nativeBuildInputs","/nix/store/msy4kfrb732qyf5zs2f42vc2hwsdg4jc-curl-7.39.0"),("out","/nix/store/xnxx6mjaddl2xi6mj4ail8sy3wp786iw-genode-toolchain-14.11-x86_64.tar.bz2"),("outputHash","0xjwrvnlmcq3p8v4mdj20gwba1qd1bk4khs1q1cbrzl24vqa1l85"),("outputHashAlgo","sha256"),("outputHashMode","flat"),("postFetch",""),("preferHashedMirrors","1"),("preferLocalBuild","1"),("propagatedBuildInputs",""),("propagatedNativeBuildInputs",""),("showURLs",""),("stdenv","/nix/store/2xppxfdg15ra07izp3ww1ddqfywiwf05-stdenv"),("system","x86_64-linux"),("urls","mirror://sourceforge/genode/genode-toolchain/14.11/genode-toolchain-14.11-x86_64.tar.bz2")])</inline>
					</dir>
				</dir>
			</content>
		</config>
		<provides><service name="File_system"/></provides>
	</start>
	<start name="log_terminal">
		<resource name="RAM" quantum="8M"/>
		<provides><service name="Terminal"/></provides>
	</start>
	<start name="nichts_store">
		<resource name="RAM" quantum="48M"/>
		<config timeout="10"/>
		<provides><service name="Nichts_store"/></provides>
	</start>
	<start name="nichts_client">
		<resource name="RAM" quantum="48M"/>
		<config>
		<drv>/nix/store/0xfzj12nd1si06xsh4hdn8bdg0pr689n-genode-toolchain-14.11-x86_64.tar.bz2.drv</drv>
		</config>
	</start>
</config>
   '';

  testScript =
    ''
      append qemu_args "-m 256"
      run_genode_until "child \"nichts_client\" exited with exit value 0"  30
    '';
}
