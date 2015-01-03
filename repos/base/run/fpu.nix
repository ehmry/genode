{ run, pkgs, runtime }:

with pkgs;

run rec {
  name = "fpu";

  contents = [
    { target = "/"; source = test.fpu; }
    { target = "/config";
      source = runtime.mkInitConfig {
        inherit name; components = [ pkgs.test.fpu ];
      };
    }
  ];

  testScript = ''
    append qemu_args "-nographic -m 64"

    run_genode_until "test done.*\n" 60

    grep_output {^\[init -\> test-fpu\] FPU}

    compare_output_to {
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
        [init -> test-fpu] FPU user started
    }
  '';
}
