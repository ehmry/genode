# \brief  Test to start threads on all available CPUs
# \author Norman Feske
# \author Alexander Boettcher
#

{ tool, base, os }:

tool.run rec {
  name = "affinity";
  
  bootInputs = [
    base.core os.init base.test.affinity
    (builtins.toFile "config" ''
	<config>
		<parent-provides>
			<service name="LOG"/>
			<service name="CPU"/>
			<service name="RM"/>
			<service name="CAP"/>
			<service name="SIGNAL"/>
			<service name="RAM"/>
			<service name="ROM"/>
			<service name="IO_MEM"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> </any-service>
		</default-route>
		<start name="test-affinity">
			<resource name="RAM" quantum="10M"/>
		</start>
	</config>
    '')
  ];

  /*
  config = tool.initConfig {
    parentProvides =
      [ "LOG" "CPU" "RM" "CAP"
       "SIGNAL" "RAM" "ROM" "IO_MEM"
      ];

      defaultRoute = [ [ "any-service" "parent" ] ];

      children = {
        "test-affinity" =
	  { #binary = base.test.affinity;
            resources = [ { name = "RAM"; quantum = "10M"; } ];
          };
      };
  };
  */

  wantCpusX = "4";
  wantCpusY = "1";
  wantCpusTotal = "4";
  rounds = "03";

  qemuArgs = "-smp ${wantCpusTotal},cores=${wantCpusTotal}";

  waitRegex = "Round ${rounds}:.*\n";
  waitTimeout = 120;
}