{ run, pkgs }:

#if {[have_spec always_hybrid]} {
#	puts "Run script does not support hybrid Linux/Genode."; exit 0 }

with pkgs;

run {
  name = "ldso";

  contents = [
    { target = "/"; source = test.ldso; }
    { target = "/"; source = libs.test-ldso_lib_1; }
    { target = "/"; source = libs.test-ldso_lib_2; }
    { target = "/"; source = libs.test-ldso_lib_dl; }
    { target = "/"; source = libs.libc; }
    { target = "/"; source = libs.libm; }

     #build "core init test/ldso test/ldso/dl"

    { target = "/config";
      source = builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="ROM"/>
			<service name="LOG"/>
			<service name="RM" />
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test-ldso">
			<resource name="RAM" quantum="2M"/>
			<config ld_bind_now="no" ld_verbose="no">
				<libc stdout="/dev/log">
					<vfs> <dir name="dev"> <log/> </dir> </vfs>
				</libc>
			</config>
		</start>
	</config>
      '';
    }
  ];

#	core init test-ldso test-ldso_lib_1.lib.so
#	test-ldso_lib_2.lib.so test-ldso_lib_dl.lib.so
#	ld.lib.so libc.lib.so libm.lib.so

  testScript = ''
append qemu_args "-nographic -m 64"

run_genode_until {child ".*" exited with exit value 123.*\n} 10

# pay only attention to the output of init and its children
grep_output {^\[init }

compare_output_to {
[init -> test-ldso] Lib_2_global 11223343
[init -> test-ldso] Lib_1_global_1 5060707
[init -> test-ldso] Lib_1_global_2 1020303
[init -> test-ldso] lib_1_attr_constructor_2 4030200f
[init -> test-ldso] lib_1_attr_constructor_1 8070604f
[init -> test-ldso] Global_1 5060707
[init -> test-ldso] Global_2 1020303
[init -> test-ldso] attr_constructor_2 4030200f
[init -> test-ldso] attr_constructor_1 8070604f
[init -> test-ldso]
[init -> test-ldso] Dynamic-linker test
[init -> test-ldso] ===================
[init -> test-ldso]
[init -> test-ldso] Global objects and local static objects of program
[init -> test-ldso] --------------------------------------------------
[init -> test-ldso] global_1 5060706
[init -> test-ldso] global_2 1020302
[init -> test-ldso] Local_1 5060707f
[init -> test-ldso] local_1 5060707e
[init -> test-ldso] Local_2 1020303f
[init -> test-ldso] local_2 1020303e
[init -> test-ldso] pod_1 8070604e
[init -> test-ldso] pod_2 4030200e
[init -> test-ldso]
[init -> test-ldso] Access shared lib from program
[init -> test-ldso] ------------------------------
[init -> test-ldso] lib_2_global 11223342
[init -> test-ldso] Lib_1_local_3 12345677
[init -> test-ldso] lib_1_local_3 12345676
[init -> test-ldso] lib_1_pod_1 8070604d
[init -> test-ldso] Libc::read:
[init -> test-ldso] no plugin found for read(0)
[init -> test-ldso] Libc::abs(-10): 10
[init -> test-ldso]
[init -> test-ldso] Catch exceptions in program
[init -> test-ldso] ---------------------------
[init -> test-ldso] exception in remote procedure call:
[init -> test-ldso] Could not open file "unknown_file"
[init -> test-ldso] caught
[init -> test-ldso] exception in program: caught
[init -> test-ldso] exception in shared lib: caught
[init -> test-ldso] exception in dynamic linker: caught
[init -> test-ldso]
[init -> test-ldso] global objects and local static objects of shared lib
[init -> test-ldso] -----------------------------------------------------
[init -> test-ldso] lib_1_global_1 5060706
[init -> test-ldso] lib_1_global_2 1020302
[init -> test-ldso] Lib_1_local_1 5060707f
[init -> test-ldso] lib_1_local_1 5060707e
[init -> test-ldso] Lib_1_local_2 1020303f
[init -> test-ldso] lib_1_local_2 1020303e
[init -> test-ldso] lib_1_pod_1 8070604e
[init -> test-ldso] lib_1_pod_2 4030200e
[init -> test-ldso]
[init -> test-ldso] Access shared lib from another shared lib
[init -> test-ldso] -----------------------------------------
[init -> test-ldso] lib_2_global 11223341
[init -> test-ldso] Lib_2_local 55667787
[init -> test-ldso] lib_2_local 55667786
[init -> test-ldso] lib_2_pod_1 87654320
[init -> test-ldso]
[init -> test-ldso] Catch exceptions in shared lib
[init -> test-ldso] ------------------------------
[init -> test-ldso] exception in lib: caught
[init -> test-ldso] exception in another shared lib: caught
[init -> test-ldso]
[init -> test-ldso] Test stack alignment
[init -> test-ldso] --------------------
[init -> test-ldso] <warning: unsupported format string argument>
[init -> test-ldso] <warning: unsupported format string argument>
[init -> test-ldso] <warning: unsupported format string argument>
[init -> test-ldso] <warning: unsupported format string argument>
[init -> test-ldso]
[init -> test-ldso] Dynamic cast
[init -> test-ldso] ------------
[init -> test-ldso] 'Object' called: good
[init -> test-ldso]
[init -> test-ldso] Destruction
[init -> test-ldso] -----------
[init -> test-ldso] ~Lib_2_local 55667785
[init -> test-ldso] ~Lib_1_local_2 1020303d
[init -> test-ldso] ~Lib_1_local_1 5060707d
[init -> test-ldso] ~Lib_1_local_3 12345675
[init -> test-ldso] ~Local_2 1020303d
[init -> test-ldso] ~Local_1 5060707d
[init -> test-ldso] attr_destructor_2 4030200d
[init -> test-ldso] attr_destructor_1 8070604c
[init -> test-ldso] ~Global_2 1020301
[init -> test-ldso] ~Global_1 5060705
[init -> test-ldso] ~Lib_1_global_2 1020301
[init -> test-ldso] ~Lib_1_global_1 5060705
[init -> test-ldso] ~Lib_2_global 11223340
}
  '';

}
