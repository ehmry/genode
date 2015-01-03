{ runtime, pkgs }:

runtime.test {
  name = "util_mmio";

  components = [ pkgs.test.util_mmio ];

  testScript =
    ''
      #
      # Execution
      #

      run_genode_until "Test done.*\n" 10

      #
      # Conclusion
      #

      grep_output {\[init -\> test-util_mmio\]}

      compare_output_to {
      	[init -> test-util_mmio] Test done
      }
    '';
}
