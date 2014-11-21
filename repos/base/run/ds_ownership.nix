{ spec, run, pkgs }:

if spec.kernel != "nova" then null else

run {
  name = "ds_ownership";

  contents = [
    { target = "/"; source = pkgs.test.ds_ownership; }
    { target = "config";
      source = builtins.toFile "config" ''
        <config verbose="yes">
          <parent-provides>
            <service name="ROM"/>
            <service name="RAM"/>
            <service name="CAP"/>
            <service name="PD"/>
            <service name="RM"/>
            <service name="CPU"/>
            <service name="LOG"/>
          </parent-provides>
          <start name="test-ds_ownership">
            <resource name="RAM" quantum="10M"/>
            <route><any-service><parent/></any-service></route>
            <config verbose="yes">
              <parent-provides>
                <service name="ROM"/>
                <service name="RAM"/>
                <service name="CAP"/>
                <service name="PD"/>
                <service name="RM"/>
                <service name="CPU"/>
                <service name="LOG"/>
              </parent-provides>
            </config>
          </start>
        </config>
      '';
    }
  ];

  testScript =
    ''
      append qemu_args "-nographic -m 64"

      run_genode_until {.*Test ended.*\.} 10

      grep_output {\[init -\> test-ds_ownership\] Test ended}

      compare_output_to {
          [init -> test-ds_ownership] Test ended successfully.
      }
    '';
}
