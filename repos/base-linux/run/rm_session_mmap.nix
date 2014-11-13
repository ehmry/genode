{ run, pkgs }:

with pkgs;

run {
  name = "rm_session_mmap";

  contents = [
    { target = "/"; source = test.rm_session_mmap; }
    { target = "/"; source = driver.timer; }
    { target = "/"; source = test.signal; }
    { target = "/config";
      source = builtins.toFile "config" ''
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
          <start name="test-rm_session_mmap">
            <resource name="RAM" quantum="2M"/>
          </start>

          <!-- add signal test for concurrency -->
          <start name="timer">
            <resource name="RAM" quantum="1M"/>
            <provides><service name="Timer"/></provides>
          </start>
          <start name="test-signal">
            <resource name="RAM" quantum="1M"/>
          </start>
        </config>
      '';
    }
  ];

  testScript = ''
    run_genode_until {.* signalling test finished .*} 60

    grep_output {^\[init -\> test-rm_session_mmap\] .* pages allocated}
    compare_output_to {
        [init -> test-rm_session_mmap] 1 of 16 pages allocated
        [init -> test-rm_session_mmap] 2 of 16 pages allocated
        [init -> test-rm_session_mmap] 3 of 16 pages allocated
        [init -> test-rm_session_mmap] 4 of 16 pages allocated
        [init -> test-rm_session_mmap] 5 of 16 pages allocated
        [init -> test-rm_session_mmap] 6 of 16 pages allocated
        [init -> test-rm_session_mmap] 7 of 16 pages allocated
        [init -> test-rm_session_mmap] 8 of 16 pages allocated
        [init -> test-rm_session_mmap] 9 of 16 pages allocated
        [init -> test-rm_session_mmap] 10 of 16 pages allocated
        [init -> test-rm_session_mmap] 11 of 16 pages allocated
        [init -> test-rm_session_mmap] 12 of 16 pages allocated
        [init -> test-rm_session_mmap] 13 of 16 pages allocated
        [init -> test-rm_session_mmap] 14 of 16 pages allocated
        [init -> test-rm_session_mmap] 15 of 16 pages allocated
        [init -> test-rm_session_mmap] 16 of 16 pages allocated
    }
  '';
}
