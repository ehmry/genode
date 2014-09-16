# \brief  Test to start threads on all available CPUs
# \author Norman Feske
# \author Alexander Boettcher
#

{ run }:
{ affinity }:

let
  wantCpusX = "4";
  wantCpusY = "1";
  wantCpusTotal = "4";
  rounds = "03";
in
run {
  name = "affinity";
  
  contents = [
    { target = "/"; source = affinity; }
    { target = "/config";
      source = builtins.toFile "config" ''
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
      '';
    }
  ];

  qemuArgs = "-smp ${wantCpusTotal},cores=${wantCpusTotal}";

  waitRegex = "Round ${rounds}:.*\n";
  waitTimeout = 120;
}