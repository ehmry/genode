{ run, pkgs }:

run {
  name = "new_delete";

  contents = [
    { target = "/"; source = pkgs.test.new_delete; }
    { target = "/config";
      source = builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="LOG"/>
			<service name="RM"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test-new_delete">
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
      '';
    }
  ];

  testScript = ''
append qemu_args "-nographic -m 64"

run_genode_until {child "test-new_delete" exited with exit value 0.*\n} 15

grep_output  {^\[init -> test-new_delete\]}

compare_output_to {
	[init -> test-new_delete] Allocator::alloc()
	[init -> test-new_delete]   A
	[init -> test-new_delete]   C
	[init -> test-new_delete]   B
	[init -> test-new_delete]   D
	[init -> test-new_delete]   E
	[init -> test-new_delete]   ~E
	[init -> test-new_delete]   ~D
	[init -> test-new_delete]   ~B
	[init -> test-new_delete]   ~C
	[init -> test-new_delete]   ~A
	[init -> test-new_delete] Allocator::free()
	[init -> test-new_delete] Allocator::alloc()
	[init -> test-new_delete]   A
	[init -> test-new_delete]   C
	[init -> test-new_delete]   B
	[init -> test-new_delete]   D
	[init -> test-new_delete]   E
	[init -> test-new_delete] throw exception
	[init -> test-new_delete]   ~D
	[init -> test-new_delete]   ~B
	[init -> test-new_delete]   ~C
	[init -> test-new_delete]   ~A
	[init -> test-new_delete] exception caught
	[init -> test-new_delete] Allocator::alloc()
	[init -> test-new_delete]   A
	[init -> test-new_delete]   C
	[init -> test-new_delete]   B
	[init -> test-new_delete]   D
	[init -> test-new_delete]   E
	[init -> test-new_delete]   ~E
	[init -> test-new_delete]   ~D
	[init -> test-new_delete]   ~B
	[init -> test-new_delete]   ~C
	[init -> test-new_delete]   ~A
	[init -> test-new_delete] Allocator::free()
	[init -> test-new_delete] Allocator::alloc()
	[init -> test-new_delete]   A
	[init -> test-new_delete]   C
	[init -> test-new_delete]   B
	[init -> test-new_delete]   D
	[init -> test-new_delete]   E
	[init -> test-new_delete] throw exception
	[init -> test-new_delete]   ~D
	[init -> test-new_delete]   ~B
	[init -> test-new_delete]   ~C
	[init -> test-new_delete]   ~A
	[init -> test-new_delete] exception caught
}
  '';
}
