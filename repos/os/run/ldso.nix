{ run, pkgs }:

with pkgs;

#if {[have_spec always_hybrid]} {
#	puts "Run script does not support hybrid Linux/Genode."; exit 0 }

run {
  name= "ldso";

  contents = [
    { target = "/"; source = test.ldso; }
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
			<config>
				<libc stdout="/dev/log">
					<vfs> <dir name="dev"> <log/> </dir> </vfs>
				</libc>
			</config>
		</start>
	</config>
      '';
    }
  ];

  testScript = ''
    append qemu_args "-nographic -m 64"

    run_genode_until "child exited with exit value 123.*\n" 10

    # pay only attention to the output of init and its children
    grep_output {^\[init }

    compare_output_to {
    [init -> test-ldso] Lib_2_global 11223343
        [ -> test-ldso] Lib_1_global_1 5060707
    [ -> test-ldso] Lib_1_global_2 1020303
    [ -> test-ldso] lib_1_attr_constructor_2 4030200f
    [ -> test-ldso] lib_1_attr_constructor_1 8070604f
    [ -> test-ldso] Global_1 5060707
    [ -> test-ldso] Global_2 1020303
    [ -> test-ldso] attr_constructor_2 4030200f
    [ -> test-ldso] attr_constructor_1 8070604f
    [ -> test-ldso] 
    [ -> test-ldso] Dynamic-linker test
    [ -> test-ldso] ===================
    [ -> test-ldso] 
    [ -> test-ldso] Global objects and local static objects of program
    [ -> test-ldso] --------------------------------------------------
    [ -> test-ldso] global_1 5060706
    [ -> test-ldso] global_2 1020302
    [ -> test-ldso] Local_1 5060707f
    [ -> test-ldso] local_1 5060707e
    [ -> test-ldso] Local_2 1020303f
    [ -> test-ldso] local_2 1020303e
    [ -> test-ldso] pod_1 8070604e
    [ -> test-ldso] pod_2 4030200e
    [ -> test-ldso] 
    [ -> test-ldso] Access shared lib from program
    [ -> test-ldso] ------------------------------
    [ -> test-ldso] lib_2_global 11223342
    [ -> test-ldso] Lib_1_local_3 12345677
    [ -> test-ldso] lib_1_local_3 12345676
    [ -> test-ldso] lib_1_pod_1 8070604d
    [ -> test-ldso] Libc::read:
    [ -> test-ldso] no plugin found for read(0)
    [ -> test-ldso] Libc::abs(-10): 10
    [ -> test-ldso] 
    [ -> test-ldso] Catch exceptions in program
    [ -> test-ldso] ---------------------------
    [ -> test-ldso] exception in remote procedure call:
    [ -> test-ldso] Could not open file "unknown_file"
    [ -> test-ldso] caught
    [ -> test-ldso] exception in program: caught
    [ -> test-ldso] exception in shared lib: caught
    [ -> test-ldso] exception in dynamic linker: caught
    [ -> test-ldso] 
    [ -> test-ldso] global objects and local static objects of shared lib
    [ -> test-ldso] -----------------------------------------------------
    [ -> test-ldso] lib_1_global_1 5060706
    [ -> test-ldso] lib_1_global_2 1020302
    [ -> test-ldso] Lib_1_local_1 5060707f
    [ -> test-ldso] lib_1_local_1 5060707e
    [ -> test-ldso] Lib_1_local_2 1020303f
    [ -> test-ldso] lib_1_local_2 1020303e
    [ -> test-ldso] lib_1_pod_1 8070604e
    [ -> test-ldso] lib_1_pod_2 4030200e
    [ -> test-ldso] 
    [ -> test-ldso] Access shared lib from another shared lib
    [ -> test-ldso] -----------------------------------------
    [ -> test-ldso] lib_2_global 11223341
    [ -> test-ldso] Lib_2_local 55667787
    [ -> test-ldso] lib_2_local 55667786
    [ -> test-ldso] lib_2_pod_1 87654320
    [ -> test-ldso] 
    [ -> test-ldso] Catch exceptions in shared lib
    [ -> test-ldso] ------------------------------
    [ -> test-ldso] exception in lib: caught
    [ -> test-ldso] exception in another shared lib: caught
    [ -> test-ldso] 
    [ -> test-ldso] test stack alignment
    [ -> test-ldso] --------------------
    [ -> test-ldso] <warning: unsupported format string argument>
    [ -> test-ldso] <warning: unsupported format string argument>
    [ -> test-ldso] <warning: unsupported format string argument>
    [ -> test-ldso] <warning: unsupported format string argument>
    [ -> test-ldso] 
    [ -> test-ldso] ~Lib_2_local 55667785
    [ -> test-ldso] ~Lib_1_local_2 1020303d
    [ -> test-ldso] ~Lib_1_local_1 5060707d
    [ -> test-ldso] ~Lib_1_local_3 12345675
    [ -> test-ldso] ~Local_2 1020303d
    [ -> test-ldso] ~Local_1 5060707d
    [ -> test-ldso] attr_destructor_2 4030200d
    [ -> test-ldso] attr_destructor_1 8070604c
    [ -> test-ldso] ~Global_2 1020301
    [ -> test-ldso] ~Global_1 5060705
    [ -> test-ldso] ~Lib_1_global_2 1020301
    [ -> test-ldso] ~Lib_1_global_1 5060705
    [ -> test-ldso] ~Lib_2_global 11223340
    }
  '';

}

